/*
This file is part of NppExec
Copyright (C) 2013 DV <dvv81 (at) ukr (dot) net>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "NppExecEngine.h"
//#include "NppExec.h"
#include "DlgConsole.h"
#include "DlgConsoleEncoding.h"
#include "DlgInputBox.h"
#include "c_base/MatchMask.h"
#include "c_base/int2str.h"
#include "c_base/str2int.h"
#include "c_base/str_func.h"
#include "c_base/HexStr.h"
#include "cpp/StrSplitT.h"
#include "CSimpleLogger.h"
#include "DirFileLister.h"
#include "fparser/fparser.hh"
#include "menuCmdID.h"
#include <stdio.h>
#include <shellapi.h>
#include <limits>

#ifdef UNICODE
  #define _t_sprintf  swprintf
  #ifndef __MINGW32__
    #define _t_str2f(x) _wtof(x)
  #else
    #define _t_str2f(x) wcstof(x, 0)
  #endif
#else
  #define _t_sprintf  sprintf
  #define _t_str2f(x) atof(x)
#endif

#define MAX_VAR_LENGTH2SHOW 200

const TCHAR MACRO_FILE_FULLPATH[]       = _T("$(FULL_CURRENT_PATH)");
const TCHAR MACRO_FILE_DIRPATH[]        = _T("$(CURRENT_DIRECTORY)");
const TCHAR MACRO_FILE_FULLNAME[]       = _T("$(FILE_NAME)");
const TCHAR MACRO_FILE_NAMEONLY[]       = _T("$(NAME_PART)");
const TCHAR MACRO_FILE_EXTONLY[]        = _T("$(EXT_PART)");
const TCHAR MACRO_NPP_DIRECTORY[]       = _T("$(NPP_DIRECTORY)");
const TCHAR MACRO_CURRENT_WORD[]        = _T("$(CURRENT_WORD)");
const TCHAR MACRO_CURRENT_LINE[]        = _T("$(CURRENT_LINE)");
const TCHAR MACRO_CURRENT_COLUMN[]      = _T("$(CURRENT_COLUMN)");
const TCHAR MACRO_DOCNUMBER[]           = _T("$(#");
const TCHAR MACRO_SYSVAR[]              = _T("$(SYS.");
const TCHAR MACRO_LEFT_VIEW_FILE[]      = _T("$(LEFT_VIEW_FILE)");
const TCHAR MACRO_RIGHT_VIEW_FILE[]     = _T("$(RIGHT_VIEW_FILE)");
const TCHAR MACRO_PLUGINS_CONFIG_DIR[]  = _T("$(PLUGINS_CONFIG_DIR)");
const TCHAR MACRO_CURRENT_WORKING_DIR[] = _T("$(CWD)");
const TCHAR MACRO_ARGC[]                = _T("$(ARGC)");
const TCHAR MACRO_ARGV[]                = _T("$(ARGV");
const TCHAR MACRO_RARGV[]               = _T("$(RARGV");
const TCHAR MACRO_INPUT[]               = _T("$(INPUT)");
const TCHAR MACRO_INPUTFMT[]            = _T("$(INPUT[%d])");
const TCHAR MACRO_EXITCODE[]            = _T("$(EXITCODE)");
const TCHAR MACRO_PID[]                 = _T("$(PID)");
const TCHAR MACRO_OUTPUT[]              = _T("$(OUTPUT)");
const TCHAR MACRO_OUTPUT1[]             = _T("$(OUTPUT1)");
const TCHAR MACRO_OUTPUTL[]             = _T("$(OUTPUTL)");
const TCHAR MACRO_MSG_RESULT[]          = _T("$(MSG_RESULT)");
const TCHAR MACRO_MSG_WPARAM[]          = _T("$(MSG_WPARAM)");
const TCHAR MACRO_MSG_LPARAM[]          = _T("$(MSG_LPARAM)");
const TCHAR MACRO_EXIT_CMD[]            = _T("$(@EXIT_CMD)");
const TCHAR MACRO_EXIT_CMD_SILENT[]     = _T("$(@EXIT_CMD_SILENT)");
const TCHAR MACRO_LAST_CMD_RESULT[]     = _T("$(LAST_CMD_RESULT)");
const TCHAR MACRO_CLIPBOARD_TEXT[]      = _T("$(CLIPBOARD_TEXT)");
const TCHAR MACRO_NPP_HWND[]            = _T("$(NPP_HWND)");
const TCHAR MACRO_SCI_HWND[]            = _T("$(SCI_HWND)");

// NppExec's Search Flags for sci_find and sci_replace:
#define NPE_SF_MATCHCASE    0x00000001 // "text" finds only "text", not "Text" or "TEXT"
#define NPE_SF_WHOLEWORD    0x00000010 // "word" finds only "word", not "sword" or "words" 
#define NPE_SF_WORDSTART    0x00000020 // "word" finds "word" and "words", not "sword"
#define NPE_SF_REGEXP       0x00000100 // search using a regular expression
#define NPE_SF_POSIX        0x00000200 // search using a POSIX-compatible regular expression
#define NPE_SF_CXX11REGEX   0x00000400 // search using a C++11 regular expression
#define NPE_SF_BACKWARD     0x00010000 // search backward (from the bottom to the top)
#define NPE_SF_NEXT         0x00020000 // search from current_position + 1
#define NPE_SF_INSELECTION  0x00100000 // search only in the selected text
#define NPE_SF_INWHOLETEXT  0x00200000 // search in the whole text, not only from the current position
#define NPE_SF_SETPOS       0x01000000 // move the caret to the position of the occurrence found
#define NPE_SF_SETSEL       0x02000000 // move the caret + select the occurrence found
#define NPE_SF_REPLACEALL   0x10000000 // replace all the occurrences from the current pos to the end
#define NPE_SF_PRINTALL     0x20000000 // print all the occurrences from the current pos to the end

enum eParamType {
    PT_UNKNOWN = 0,
    PT_INT,
    PT_PINT,
    PT_STR,
    PT_PSTR,
    PT_HEXSTR,
    PT_PHEXSTR,

    PT_COUNT
};

const TCHAR* STR_PARAMTYPE[PT_COUNT] =
{
    _T(""),
    _T("int"),
    _T("@int"),
    _T("str"),
    _T("@str"),
    _T("hex"),
    _T("@hex")
};

extern FuncItem              g_funcItem[nbFunc + MAX_USERMENU_ITEMS + 1];

extern COLORREF              g_colorTextNorm;

extern CInputBoxDlg InputBoxDlg;

BOOL     g_bIsNppUnicode = FALSE;
WNDPROC  nppOriginalWndProc;

typedef std::map<tstr, tstr> tEnvVars;
tEnvVars g_GlobalEnvVars;
tEnvVars g_LocalEnvVarNames;
CCriticalSection g_csEnvVars;

static int  FileFilterPos(const TCHAR* szFilePath);
static void GetPathAndFilter(const TCHAR* szPathAndFilter, int nFilterPos, tstr& out_Path, tstr& out_Filter);
static void GetFilePathNamesList(const TCHAR* szPath, const TCHAR* szFilter, CListT<tstr>& FilesList);
static bool PrintDirContent(CNppExec* pNppExec, const TCHAR* szPath, const TCHAR* szFilter);
static void runInputBox(CScriptEngine* pScriptEngine, const TCHAR* szMessage);


class PrintMacroVarFunc
{
    public:
        typedef const CNppExecMacroVars::tMacroVars container_type;
        typedef container_type::const_iterator iterator_type;

        PrintMacroVarFunc(CNppExec* pNppExec) : m_pNppExec(pNppExec)
        {
        }

        bool operator()(iterator_type itrVar, bool isLocalVar)
        {
            tstr S = isLocalVar ? _T("local ") : _T("");
            S += itrVar->first;
            S += _T(" = ");
            if ( itrVar->second.length() > MAX_VAR_LENGTH2SHOW )
            {
                S.Append( itrVar->second.c_str(), MAX_VAR_LENGTH2SHOW - 5 );
                S += _T("(...)");
            }
            else
            {
                S += itrVar->second;
            }
            m_pNppExec->GetConsole().PrintMessage( S.c_str(), false );

            return true;
        }

    protected:
        CNppExec* m_pNppExec;
};

class SubstituteMacroVarFunc
{
    public:
        typedef const CNppExecMacroVars::tMacroVars container_type;
        typedef container_type::const_iterator iterator_type;

        SubstituteMacroVarFunc(tstr& Value) : m_Value(Value)
        {
            m_ValueUpper = Value;
            NppExecHelpers::StrUpper(m_ValueUpper);
        }

        SubstituteMacroVarFunc& operator=(const SubstituteMacroVarFunc&) = delete;

        bool operator()(iterator_type itrVar, bool /*isLocalVar*/)
        {
            int pos = m_ValueUpper.Find(_T("$("), 0);
            if ( pos < 0 )
                return false;

            const tstr& varName = itrVar->first;
            while ( (pos = m_ValueUpper.Find(varName, pos)) >= 0 )
            {
                const tstr& varValue = itrVar->second;
                m_ValueUpper.Replace( pos, varName.length(), varValue.c_str(), varValue.length() );
                m_Value.Replace( pos, varName.length(), varValue.c_str(), varValue.length() );
                pos += varValue.length();
            }

            return true;
        }

    protected:
        tstr& m_Value;
        tstr m_ValueUpper;
};

template<class MacroVarFunc> void IterateUserMacroVars(
    typename MacroVarFunc::container_type& userMacroVars,
    typename MacroVarFunc::container_type& userLocalMacroVars,
    MacroVarFunc func)
{
    bool isLocalVars = true;
    typename MacroVarFunc::iterator_type itrVar = userLocalMacroVars.begin();
    for ( ; ; ++itrVar )
    {
        if ( isLocalVars )
        {
            if ( itrVar == userLocalMacroVars.end() )
            {
                itrVar = userMacroVars.begin();
                if ( itrVar != userMacroVars.end() )
                    isLocalVars = false;
                else
                    break;
            }
        }
        else
        {
            if ( itrVar == userMacroVars.end() )
                break;
        }

        if ( isLocalVars ||
             (userLocalMacroVars.find(itrVar->first) == userLocalMacroVars.end()) )
        {
            if ( !func(itrVar, isLocalVars) )
                break;
        }
    }
}


#define abs_val(x) (((x) < 0) ? (-(x)) : (x))


static bool IsTabSpaceOrEmptyChar(const TCHAR ch)
{
  return ( (ch == 0 || ch == _T(' ') || ch == _T('\t')) ? true : false );
}

/*
static bool isHexNumChar(const TCHAR ch)
{
    return ( ( (ch >= _T('0') && ch <= _T('9')) ||
               (ch >= _T('A') && ch <= _T('F')) ||
               (ch >= _T('a') && ch <= _T('f')) ) ? true : false );
}*/
/**/
static bool isDecNumChar(const TCHAR ch)
{
    return ( (ch >= _T('0') && ch <= _T('9')) ? true : false );
}

/**/
#define  SEP_TABSPACE  0

// gets the current param and returns a pointer to the next param
const TCHAR* get_param(const TCHAR* s, tstr& param, const TCHAR sep = SEP_TABSPACE, 
                       bool* pDblQuote = 0, bool* pBracket = 0)
{
    param.Clear();

    if ( pDblQuote )  *pDblQuote = false;
    if ( pBracket )   *pBracket = false;

    while ( NppExecHelpers::IsTabSpaceChar(*s) )  ++s;  // skip leading tabs/spaces

    int i = 0;
    int n = 0;
    bool isComplete = false;
    bool isDblQuote = false;
    bool isEnvVar = false;
    bool isBracket = false;

    if ( pBracket )
    {
        if ( *s == _T('@') )
        {
            param += _T('@');
            ++s;
        }
        if ( *s == _T('[') )
        {
            isBracket = true;
            *pBracket = true;
            param += _T('[');
            ++s;
        }
    }

    while ( !isComplete )
    {
        const TCHAR ch = *s;
        
        switch ( ch )
        {
            case _T('\"'):
                if ( !isEnvVar )
                {
                    isDblQuote = !isDblQuote;
                    if ( isDblQuote && pDblQuote )
                    {
                        *pDblQuote = true;
                    }
                    if ( isBracket )
                        param += ch;
                }
                else
                {
                    param += ch;
                }
                ++s; // to next character
                break;

            case _T(' '):
            case _T('\t'):
                if ( sep == SEP_TABSPACE && !isDblQuote && !isBracket && !isEnvVar )
                {
                    isComplete = true;
                }
                else
                {
                    param += ch;
                    if ( isDblQuote || isBracket )
                        n = param.length();
                }
                ++s; // to next character
                break;

            case _T('$'):
                if ( !isEnvVar )
                {
                    if ( *(s + 1) == _T('(') )
                    {
                        isEnvVar = true;
                    }
                }
                param += ch;
                ++s; // to next character
                break;

            case _T(')'):
                if ( isEnvVar )
                {
                    isEnvVar = false;
                }
                param += ch;
                ++s; // to next character
                break;

            case _T(']'):
                if ( isBracket && !isEnvVar && !isDblQuote )
                {
                    isBracket = false;
                }
                param += ch;
                ++s; // to next character
                break;

            case 0:
                isComplete = true;
                break;

            default:
                if ( ch == sep && !isDblQuote && !isEnvVar )
                {
                    isComplete = true;
                }
                else
                {
                    param += ch;
                }
                ++s; // to next character
                break;
        }
    }
    
    for ( i = param.length() - 1; i >= n; i-- )
    {
        if ( !NppExecHelpers::IsTabSpaceChar(param[i]) )
            break;
    }
    n = param.length() - 1;
    if ( i < n )
    {
        param.Delete(i + 1, n - i); // remove trailing tabs/spaces
    }

    while ( NppExecHelpers::IsTabSpaceChar(*s) )  ++s;  // skip trailing tabs/spaces

    return s;
}

static tstr substituteMacroVarsIfNotDelayed(CScriptEngine* pScriptEngine, const tstr& params, bool bUseDelayedSubstitution)
{
    tstr key;
    const TCHAR* p = get_param( params.c_str(), key, SEP_TABSPACE );
    if ( (*p != 0) && (*p != _T('=')) && !key.IsEmpty() )
    {
        NppExecHelpers::StrUpper(key);
        if ( key == _T("+V") )
            bUseDelayedSubstitution = true;
        else if ( key == _T("-V") )
            bUseDelayedSubstitution = false;
        else
            p = NULL;
    }
    else
        p = NULL;

    tstr paramsToReturn = (p == NULL) ? params : tstr(p);
    if ( !bUseDelayedSubstitution )
    {
        const CStrSplitT<TCHAR> cmdArgs;
        CNppExec* pNppExec = pScriptEngine->GetNppExec();
        pNppExec->GetMacroVars().CheckCmdArgs(paramsToReturn, cmdArgs);
        pNppExec->GetMacroVars().CheckAllMacroVars(pScriptEngine, paramsToReturn, true);
    }

    return paramsToReturn;
}

static tstr getQueuedCommand(const tstr& params, bool& bUseSeparateScript)
{
    tstr key;
    const TCHAR* p1 = params.c_str(); // beginning of the key
    const TCHAR* p2 = get_param( p1, key, SEP_TABSPACE ); // end of the key
    bool bContinue = true;
    while ( bContinue )
    {
        bContinue = false;
        if ( (*p2 != 0) && !key.IsEmpty() )
        {
            NppExecHelpers::StrUpper(key);
            if ( key == _T("+V") || key == _T("-V") )
            {
                p1 = p2; // beginning of the next key
                p2 = get_param( p1, key, SEP_TABSPACE ); // end of the next key
                bContinue = true;
            }
            else if ( key == _T("+S") )
                bUseSeparateScript = true;
            else if ( key == _T("-S") )
                bUseSeparateScript = false;
            else
                p2 = NULL;
        }
        else
            p2 = NULL;
    }

    if ( p2 == NULL ) // "+S" or "-S" was not found
        return params;
    
    tstr Cmd;
    Cmd.Append(params.c_str(), static_cast<int>(p1 - params.c_str()));
    Cmd.Append(p2, params.length() - static_cast<int>(p2 - params.c_str()));
    return Cmd;
}

/**/
class FParserWrapper
{
    public:
        typedef FunctionParser fparser_type;

    public:
        typedef struct sUserConst {
            CStrT<char> constName;
            tstr        constValue;
            int         nLine;
        } tUserConst;

    public:
        FParserWrapper()
          : m_fp(nullptr)
          , m_hasConsts(false)
          , m_calc_precision(0.000001)
        {
            lstrcpy( m_szCalcDefaultFmt, _T("%.6f") );
            lstrcpy( m_szCalcSmallFmt,   _T("%.6G") );
            lstrcpy( m_szCalcBigFmt,     _T("%.7G") );
        }

        ~FParserWrapper()
        {
            if ( m_fp != nullptr )
                delete m_fp;
        }

        bool Calculate(CNppExec* pNppExec, const tstr& func, tstr& calcError, tstr& ret)
        {
            return calc2(pNppExec, func, calcError, ret);
        }

    private:
        void readConstsFromFile(CNppExec* pNppExec, const tstr& path)
        {
            CFileBufT<char>    fbuf;
            CStrT<char>        line;
            tUserConst         userConst;
            CListT<tUserConst> unparsedUserConstsList;

            if ( fbuf.LoadFromFile(path.c_str(), true, pNppExec->GetOptions().GetInt(OPTI_UTF8_DETECT_LENGTH)) )
            {
                int nItemsOK = 0;

                Runtime::GetLogger().Add(   path.c_str() );
                Runtime::GetLogger().Add(   _T("(") );
                Runtime::GetLogger().IncIndentLevel();

                while ( fbuf.GetLine(line) >= 0 )
                {
                    if ( line.length() > 0 )
                    {
                        int i = line.Find("//");
                        if ( i >= 0 )
                            line.Delete(i, -1);

                        NppExecHelpers::StrDelLeadingTabSpaces(line);

                        if ( line.length() > 0 && line.StartsWith("#define") )
                        {
                            line.Delete(0, 7); // delete "#define"
                            NppExecHelpers::StrDelLeadingTabSpaces(line);
                            if ( line.length() > 0 )
                            {
                                userConst.constName.Clear();
                                userConst.constValue.Clear();
                                userConst.nLine = fbuf.GetLineNumber() - 1;

                                i = 0;
                                while ( i < line.length() && !NppExecHelpers::IsTabSpaceChar(line[i]) )
                                {
                                    userConst.constName += line[i];
                                    ++i;
                                }

                                line.Delete(0, i); // delete const name
                                NppExecHelpers::StrDelLeadingTabSpaces(line);
                                NppExecHelpers::StrDelTrailingTabSpaces(line);
                                if ( line.length() > 0 )
                                {
                                    double val = 0;
                                    bool isVal = false;

                                  #ifdef UNICODE
                                    wchar_t* p = SysUniConv::newMultiByteToUnicode(line.c_str());
                                    if ( p )
                                    {
                                        userConst.constValue = p;
                                        delete [] p;
                                    }
                                  #else
                                    userConst.constValue = line;
                                  #endif

                                    /*if ( isDecNumChar(constValue.GetAt(0)) )
                                    {
                                    val = c_base::_tstr2uint(constValue.c_str());
                                    isVal = true;
                                    }
                                    else*/
                                    {
                                        tstr calcErr;

                                        Runtime::GetLogger().Activate(false); // temporary disable

                                        isVal = calc2(pNppExec, userConst.constValue, calcErr, userConst.constValue);

                                        Runtime::GetLogger().Activate(true);  // enable again

                                        if ( isVal )
                                        {
                                            if ( userConst.constValue.Find(_T('.')) >= 0 )
                                                val = _t_str2f(userConst.constValue.c_str());
                                            else
                                                val = c_base::_tstr2uint(userConst.constValue.c_str());
                                        }
                                        else
                                        {
                                            unparsedUserConstsList.Add(userConst);
                                        }
                                    }

                                    if ( isVal )
                                    {
                                        m_fp->AddConstant(userConst.constName.c_str(), fparser_type::value_type(val));
                                        ++nItemsOK;
                                    }
                                }
                            }
                        }
                    }
                }

                if ( !unparsedUserConstsList.IsEmpty() )
                {
                    tstr calcErr;

                    CListItemT<tUserConst>* pItem = unparsedUserConstsList.GetFirst();
                    while ( pItem )
                    {
                        tUserConst& uc = pItem->GetItem();
                        bool isVal = false;

                        Runtime::GetLogger().Activate(false); // temporary disable

                        isVal = calc2(pNppExec, uc.constValue, calcErr, uc.constValue);

                        Runtime::GetLogger().Activate(true);  // enable again

                        if ( isVal )
                        {
                            double val = 0;
                            if ( uc.constValue.Find(_T('.')) >= 0 )
                                val = _t_str2f(uc.constValue.c_str());
                            else
                                val = c_base::_tstr2uint(uc.constValue.c_str());

                            m_fp->AddConstant(uc.constName.c_str(), fparser_type::value_type(val));
                            ++nItemsOK;
                        }
                        else
                        {

                            Runtime::GetLogger().AddEx( _T("error at line %d: \"%s\""), uc.nLine, uc.constValue.c_str() );
                            Runtime::GetLogger().AddEx( _T(" - %s"), calcErr.c_str() );

                        }

                        pItem = pItem->GetNext();
                    }
                }

                Runtime::GetLogger().AddEx( _T("%d definitions added."), nItemsOK );
                Runtime::GetLogger().DecIndentLevel();
                Runtime::GetLogger().Add(   _T(")") );

            }
        }

        void initFParserConsts(CNppExec* pNppExec)
        {

            Runtime::GetLogger().Add(   _T("initFParserConsts()") );
            Runtime::GetLogger().Add(   _T("{") );
            Runtime::GetLogger().IncIndentLevel();

            m_fp->AddConstant("WM_COMMAND", fparser_type::value_type(WM_COMMAND));
            m_fp->AddConstant("WM_USER", fparser_type::value_type(WM_USER));
            m_fp->AddConstant("NPPMSG", fparser_type::value_type(NPPMSG));

            CDirFileLister FileLst;
            tstr           path;

            path = pNppExec->getPluginPath();
            path += _T("\\NppExec\\*.h");
            if ( FileLst.FindNext(path.c_str(), CDirFileLister::ESF_FILES | CDirFileLister::ESF_SORTED) )
            {
                do {
                    path = pNppExec->getPluginPath();
                    path += _T("\\NppExec\\");
                    path += FileLst.GetItem();

                    readConstsFromFile(pNppExec, path);
                } 
                while ( FileLst.GetNext() );
            }
            else
            {
                path = pNppExec->getPluginPath();
                path += _T("\\NppExec");

                Runtime::GetLogger().AddEx( _T("; no *.h files found in \"%s\""), path.c_str() );

                pNppExec->GetConsole().PrintMessage( _T("- Warning: fparser's constants have not been initialized because"), false );
                pNppExec->GetConsole().PrintMessage( _T("the following folder either does not exist or is empty:"), false );
                pNppExec->GetConsole().PrintMessage( path.c_str(), false );
            }

            Runtime::GetLogger().DecIndentLevel();
            Runtime::GetLogger().Add(   _T("}") );

        }

        static fparser_type::value_type fp_hex(const fparser_type::value_type* p)
        {
            return p[0];
        }

        void initFParserFuncs()
        {
            m_fp->AddFunction("hex", fp_hex, 1);
        }

        fparser_type::value_type calc(CNppExec* pNppExec, const tstr& func, tstr& calcError)
        {
            if ( func.IsEmpty() )
            {
                calcError = _T("Input function is empty");
                return fparser_type::value_type(0);
            }

            /*
            {
                TCHAR szNum[50];
                // pre-parse for '0x...' numbers 
                int pos = func.Find( _T("0x") );
                while ( pos >= 0 )
                {
                    const TCHAR ch = func.GetAt(pos + 2);
                    if ( isHexNumChar(ch) )
                    {
                        const int nn = c_base::_tstr2int( func.c_str() + pos );
                        int hexLen = 2;
                        while ( isHexNumChar(func.GetAt(pos + hexLen)) )  ++hexLen;
                        int decLen = c_base::_tint2str(nn, szNum);
                        func.Replace(pos, hexLen, szNum, decLen);
                        pos += decLen;
                    }
                    pos = func.Find( _T("0x"), pos );
                }
            }
            */

          #ifdef UNICODE
            char* pFunc = SysUniConv::newUnicodeToMultiByte( func.c_str() );
          #else
            const char* pFunc = func.c_str();
          #endif

            {
                CCriticalSectionLockGuard lock(m_cs);

                if ( !m_hasConsts )
                {
                    m_fp = new fparser_type();
                    int len = 0;
                    const TCHAR* pPrecision = pNppExec->GetOptions().GetStr(OPTS_CALC_PRECISION, &len);
                    if ( len > 0 )
                    {
                        double precision = _t_str2f(pPrecision);
                        precision = abs_val(precision);
                        if ( precision > 0.0 )
                        {
                            int d = 0;

                            m_calc_precision = precision;

                            while ( precision < 0.99 )
                            {
                                precision *= 10;
                                ++d;
                            }
                            wsprintf(m_szCalcDefaultFmt, _T("%%.%df"), d);
                            wsprintf(m_szCalcSmallFmt,   _T("%%.%dG"), d);
                            wsprintf(m_szCalcBigFmt,     _T("%%.%dG"), d + 1);
                        }
                    }

                    m_hasConsts = true;
                    initFParserConsts(pNppExec);
                    initFParserFuncs();
                }
            }

            int errPos;
            fparser_type::value_type ret(0);

            calcError.Clear();

            {
                CCriticalSectionLockGuard lock(m_cs);

                errPos = m_fp->Parse(pFunc, "");
                if ( errPos == -1 )
                {
                    fparser_type::value_type var(0);

                    var = m_fp->Eval( &var );
                    int err = m_fp->EvalError();
                    if ( err == 0 )
                        ret = var;
                    else
                        calcError.Format(50, _T("Eval error (%d)"), err);
                }
                else
                {
                    const char* pErr = m_fp->ErrorMsg();
                  #ifdef UNICODE
                    TCHAR* pErrW = SysUniConv::newMultiByteToUnicode( pErr );
                    calcError = pErrW; // store a copy of the error message
                    delete [] pErrW;
                  #else
                    calcError = pErr; // store a copy of the error message
                  #endif
                }
            }

            if ( errPos != -1 )
            {
                TCHAR szNum[50];

                calcError += _T(" at pos ");
                c_base::_tint2str(errPos, szNum);
                calcError += szNum;
            }

          #ifdef UNICODE
            delete [] pFunc;
          #endif

            return ret;
        }

        template<typename T> void format_result(const std::complex<T>& val, tstr& ret, const tstr& func)
        {
            if ( val.imag() == 0 )
            {
                format_result(val.real(), ret, func);
            }
            else
            {
                TCHAR szNum[120];
                // ha-ha, wsprintf does not support neither "%f" nor "%G"
                _t_sprintf(szNum, _T("%g %c %gi"), val.real(), val.imag() < 0 ? '-' : '+', std::abs(val.imag()));
                ret = szNum;
            }
        }

        template<typename T> void format_result(const T& val, tstr& ret, const tstr& func)
        {
            // Note: 
            // As we are using FunctionParser that operates with 'double',
            // its accuracy with regards to 64-bit integers is limited by
            // the mantissa of a 'double'.
          #ifndef __MINGW32__
            const __int64 max_accurate_i64_value = (1i64) << std::numeric_limits<T>::digits; // 2**53 for the 'double' type
          #else
            const long long max_accurate_i64_value = (1LL) << std::numeric_limits<T>::digits; // 2**53 for the 'double' type
          #endif

            TCHAR szNum[80];
            szNum[0] = 0;

            if ( (func.StartsWith(_T("hex(")) || func.StartsWith(_T("hex ("))) &&
                 (abs_val(val) < max_accurate_i64_value) )
            {
                unsigned __int64 n = static_cast<unsigned __int64>(val);
                _t_sprintf( szNum, _T("0x%I64X"), n );
            }
            else if ( abs_val(val) > m_calc_precision*100 )
            {
                if ( abs_val(val) < max_accurate_i64_value )
                {
                    __int64 n = static_cast<__int64>(val);
                    double  diff = static_cast<double>(val - n);

                    if ( abs_val(diff) < m_calc_precision )
                    {
                        // result can be rounded
                        _t_sprintf( szNum, _T("%I64d"), n ); 
                        // ha-ha, wsprintf does not support neither "%I64d" nor "%f"
                    }
                    else
                    {
                        int nn = _t_sprintf( szNum, m_szCalcDefaultFmt, val );
                        if ( nn > 0 )
                        {
                            while ( nn > 0 && szNum[--nn] == _T('0') ) ;
                            if ( szNum[nn] == _T('.') )  ++nn; // keep one '0' after '.'
                            szNum[nn + 1] = 0; // exclude all trailing '0' after '.'
                        }
                    }
                }
                else
                {
                    // result is too big
                    _t_sprintf( szNum, m_szCalcBigFmt, val );
                }
            }
            else
            {
                // result is too small
                _t_sprintf( szNum, m_szCalcSmallFmt, val );
                // ha-ha, wsprintf does not support neither "%f" nor "%G"
            }

            ret = szNum;
        }

        bool calc2(CNppExec* pNppExec, const tstr& func, tstr& calcError, tstr& ret)
        {
            fparser_type::value_type fret = calc(pNppExec, func, calcError);

            if ( calcError.IsEmpty() )
            {
                format_result(fret, ret, func);
            }

            return calcError.IsEmpty();
        }

    private:
        CCriticalSection m_cs;
        fparser_type* m_fp;
        bool   m_hasConsts;
        double m_calc_precision;
        TCHAR  m_szCalcDefaultFmt[12];
        TCHAR  m_szCalcSmallFmt[12];
        TCHAR  m_szCalcBigFmt[12];
};

static FParserWrapper g_fp;
/**/

/*
 * CScriptEngine
 *
 *
 * Internal Commands:
 * ------------------
 * cls
 *   - clear Console screen
 * cd
 *   - shows current path
 * cd <path>
 *   - changes current directory (absolute or relative)
 * cd <drive:\path>
 *   - changes current drive and directory
 * dir
 *   - lists subdirs and files
 * dir <mask>
 *   - lists subdirs and files matched the mask
 * dir <path\mask>
 *   - lists subdirs and files matched the mask
 * echo <text>
 *   - prints a text in the Console
 * if <condition> goto <label>
 *   - jumps to the label if the condition is true
 * if ... else if ... else ... endif
 *   - conditional execution
 * goto <label>
 *   - jumps to the label
 * set 
 *   - shows all user's variables
 * set <var> 
 *   - shows the value of user's variable <var>
 * set <var> = <value> 
 *   - sets the value of user's variable <var>
 * set <var> ~ <math expression>
 *   - calculates the math expression
 * set <var> ~ strlen <string>
 *   - calculates the string length
 * set <var> ~ strlenutf8 <string>
 *   - calculates the UTF-8 string length
 * set <var> ~ strlensci <string>
 *   - string length, using Scintilla's encoding
 * set <var> ~ strupper <string>
 *   - returns the string in upper case
 * set <var> ~ strlower <string>
 *   - returns the string in lower case
 * set <var> ~ substr <pos> <len> <string>
 *   - returns the substring
 * set <var> ~ strfind <s> <t>
 *   - returns the first position of <t> in <s>
 * set <var> ~ strrfind <s> <t>
 *   - returns the last position of <t> in <s>
 * set <var> ~ strreplace <s> <t0> <t1>
 *   - replaces all <t0> with <t1>
 * set <var> ~ strfromhex <hs>
 *   - returns a string from the hex-string
 * set <var> ~ strtohex <s>
 *   - returns a hex-string from the string
 * set local
 *   - shows all user's local variables
 * set local <var>
 *   - shows the value of user's local variable <var>
 * set local <var> = ...
 *   - sets the value of user's local variable <var>
 * set local <var> ~ ...
 *   - calculates the value of user's local variable <var>
 * unset <var>
 *   - removes user's variable <var>
 * unset local <var>
 *   - removes user's local variable <var>
 * env_set <var>
 *   - shows the value of environment variable <var>
 * env_set <var> = <value>
 *   - sets the value of environment variable <var>
 * env_unset <var>
 *   - removes/restores the environment variable <var>
 * inputbox "message"
 *   - shows InputBox, sets $(INPUT)
 * inputbox "message" : initial_value
 *   - shows InputBox with specified initial value, sets $(INPUT)
 * inputbox "message" : "value_name" : initial_value
 *   - InputBox customization
 * con_colour <colours>
 *   - sets the Console's colours
 * con_filter <filters>
 *   - enables/disables the Console's output filters
 * con_loadfrom <file> 
 *   - loads a file's content to the Console
 * con_load <file>
 *   - see "con_loadfrom"
 * con_saveto <file>
 *   - saves the Console's content to a file
 * con_save <file>
 *   - see "con_saveto"
 * sel_loadfrom <file> 
 *   - replace current selection with a file's content
 * sel_load <file> 
 *   - see "sel_loadfrom"
 * sel_saveto <file>
 *   - save the selected text (in current encoding) to a file
 * sel_saveto <file> : <encoding>
 *   - save the selected text (in specified encoding) to a file
 * sel_save <file> : <encoding>
 *   - see "sel_saveto"
 * sel_settext <text>
 *   - replace current selection with the text specified
 * sel_settext+ <text>
 *   - replace current selection with the text specified
 * text_loadfrom <file> 
 *   - replace the whole text with a file's content
 * text_load <file> 
 *   - see "text_loadfrom"
 * text_saveto <file>
 *   - save the whole text (in current encoding) to a file
 * text_saveto <file> : <encoding>
 *   - save the whole text (in specified encoding) to a file
 * text_save <file> : <encoding>
 *   - see "text_saveto"
 * clip_settext <text>
 *   - set the clipboard text
 * npp_exec <script>
 *   - executes commands from specified NppExec's script
 * npp_exec <file>
 *   - executes commands from specified NppExec's file
 *   - works with a partial file path/name
 * npp_close
 *   - closes current file in Notepad++
 * npp_close <file>
 *   - closes specified file opened in Notepad++
 *   - works with a partial file path/name
 * npp_console <on/off/keep>
 *   - show/hide the Console window
 * npp_console <enable/disable>
 *   - enable/disable output to the Console
 * npp_console <1/0/?> 
 *   - show/hide the Console window
 * npp_console <+/->
 *   - enable/disable output to the Console
 * npp_menucommand <menu\item\name>
 *   - executes (invokes) a menu item
 * npp_open <file>
 *   - opens specified file in Notepad++
 * npp_open <mask>
 *   - opens file(s) matched the mask
 * npp_open <path\mask>
 *   - opens file(s) matched the mask
 * npp_run <command> 
 *   - the same as Notepad++'s Run command
 *   - executes command (runs a child process) w/o waiting until it returns
 * npp_save 
 *   - saves current file in Notepad++
 * npp_save <file>
 *   - saves specified file in Notepad++ (if it's opened in Notepad++)
 *   - works with a partial file path/name
 * npp_saveas <file>
 *   - saves current file with a new (path)name
 * npp_saveall
 *   - saves all modified files
 * npp_switch <file> 
 *   - switches to specified file (if it's opened in Notepad++)
 *   - works with a partial file path/name
 * npp_setfocus
 *    - sets the keyboard focus
 * npp_sendmsg <msg>
 *   - sends a message (msg) to Notepad++
 * npp_sendmsg <msg> <wparam>
 *   - sends a message (msg) with parameter (wparam) to Notepad++
 * npp_sendmsg <msg> <wparam> <lparam>
 *   - sends a message (msg) with parameters (wparam, lparam) to Notepad++
 * npp_sendmsgex <hwnd> <msg> <wparam> <lparam>
 *   - sends a message (msg) with parameters (wparam, lparam) to hwnd
 * sci_sendmsg <msg>
 *   - sends a message (msg) to current Scintilla
 * sci_sendmsg <msg> <wparam>
 *   - sends a message (msg) with parameter (wparam) to current Scintilla
 * sci_sendmsg <msg> <wparam> <lparam>
 *   - sends a message (msg) with parameters (wparam, lparam) to current Scintilla
 * sci_find <flags> <find_what>
 *   - finds a string in the current editing (Scintilla's) window
 * sci_replace <flags> <find_what> <replace_with>
 *   - replaces a string in the current editing (Scintilla's) window
 * proc_signal <signal>
 *   - signal to a child process
 * sleep <ms>
 *   - sleep during ms milliseconds
 * sleep <ms> <text>
 *   - print the text and sleep during ms milliseconds
 * npe_cmdalias
 *   - show all command aliases
 * npe_cmdalias <alias>
 *   - shows the value of command alias
 * npe_cmdalias <alias> =
 *   - removes the command alias
 * npe_cmdalias <alias> = <command>
 *   - sets the command alias
 * npe_console <options>
 *   - set/modify Console options/mode
 * npe_debuglog <on/off>
 *   - enable/disable Debug Log
 * npe_noemptyvars <1/0>
 *   - enable/disable replacement of empty vars
 * npe_queue <command>
 *   - queue NppExec's command to be executed
 *
 * Internal Macros (environment variables):
 * ----------------------------------------
 * The same as here: http://notepad-plus.sourceforge.net/uk/run-HOWTO.php
 *
 * $(FULL_CURRENT_PATH)  : E:\my Web\main\welcome.html
 * $(CURRENT_DIRECTORY)  : E:\my Web\main
 * $(FILE_NAME)          : welcome.html
 * $(NAME_PART)          : welcome
 * $(EXT_PART)           : .html
 * $(NPP_DIRECTORY)      : the full path of directory with notepad++.exe
 * $(CURRENT_WORD)       : word(s) you selected in Notepad++
 * $(CURRENT_LINE)       : current line number
 * $(CURRENT_COLUMN)     : current column number
 *
 * Additional environment variables:
 * ---------------------------------
 * $(CLIPBOARD_TEXT)     : text from the clipboard
 * $(#0)                 : C:\Program Files\Notepad++\notepad++.exe
 * $(#N), N=1,2,3...     : full path of the Nth opened document
 * $(LEFT_VIEW_FILE)     : current file path-name in primary (left) view
 * $(RIGHT_VIEW_FILE)    : current file path-name in second (right) view
 * $(PLUGINS_CONFIG_DIR) : full path of the plugins configuration directory
 * $(CWD)                : current working directory of NppExec (use "cd" to change it)
 * $(ARGC)               : number of arguments passed to the NPP_EXEC command
 * $(ARGV)               : all arguments passed to the NPP_EXEC command after the script name
 * $(ARGV[0])            : script name - first parameter of the NPP_EXEC command
 * $(ARGV[N])            : Nth argument (N=1,2,3...)
 * $(RARGV)              : all arguments in reverse order (except the script name)
 * $(RARGV[N])           : Nth argument in reverse order (N=1,2,3...)
 * $(INPUT)              : this value is set by the 'inputbox' command
 * $(INPUT[N])           : Nth field of the $(INPUT) value (N=1,2,3...)
 * $(OUTPUT)             : this value can be set by the child process, see npe_console v+
 * $(OUTPUT1)            : first line in $(OUTPUT)
 * $(OUTPUTL)            : last line in $(OUTPUT)
 * $(EXITCODE)           : exit code of the last executed child process
 * $(PID)                : process id of the current (or the last) child process
 * $(LAST_CMD_RESULT)    : result of the last NppExec's command
 *                           (1 - succeeded, 0 - failed, -1 - invalid arg)
 * $(MSG_RESULT)         : result of 'npp_sendmsg[ex]' or 'sci_sendmsg'
 * $(MSG_WPARAM)         : wParam (output) of 'npp_sendmsg[ex]' or 'sci_sendmsg'
 * $(MSG_LPARAM)         : lParam (output) of 'npp_sendmsg[ex]' or 'sci_sendmsg'
 * $(NPP_HWND)           : Notepad++'s main window handle
 * $(SCI_HWND)           : current Scintilla's window handle
 * $(SYS.<var>)          : system's environment variable, e.g. $(SYS.PATH)
 * $(@EXIT_CMD)          : a callback exit command for a child process
 * $(@EXIT_CMD_SILENT)   : a silent (non-printed) callback exit command
 *
 */

CScriptEngine::CScriptCommandRegistry CScriptEngine::m_CommandRegistry;

CScriptEngine::CScriptEngine(CNppExec* pNppExec, const CListT<tstr>& CmdList, const tstr& id)
{
    m_strInstance = NppExecHelpers::GetInstanceAsString(this);

    m_pNppExec = pNppExec;
    m_CmdList.Copy(CmdList); // own m_CmdList
    m_id = id;

    m_nCmdType = CMDTYPE_UNKNOWN;
    m_nRunFlags = 0;
    m_dwThreadId = 0;
    m_bTriedExitCmd = false;
    m_isClosingConsole = false;

    Runtime::GetLogger().AddEx_WithoutOutput( _T("; CScriptEngine - create (instance = %s)"), GetInstanceStr() );
}

CScriptEngine::~CScriptEngine()
{
    Runtime::GetLogger().AddEx_WithoutOutput( _T("; CScriptEngine - destroy (instance = %s)"), GetInstanceStr() );
}

const TCHAR* CScriptEngine::GetInstanceStr() const
{
    return m_strInstance.c_str();
}

#define INVALID_TSTR_LIST_ITEM ((CListItemT<tstr>*)(-1))

void CScriptEngine::Run(unsigned int nRunFlags)
{
    m_nRunFlags = nRunFlags;
    m_dwThreadId = ::GetCurrentThreadId();
    m_bTriedExitCmd = false;
    m_isClosingConsole = false;

    if ( m_eventRunIsDone.IsNull() )
        m_eventRunIsDone.Create(NULL, TRUE, FALSE, NULL); // manual reset, non-signaled
    else
        m_eventRunIsDone.Reset();

    if ( m_eventAbortTheScript.IsNull() )
        m_eventAbortTheScript.Create(NULL, TRUE, FALSE, NULL); // manual reset, non-signaled
    else
        m_eventAbortTheScript.Reset();

    m_pNppExec->GetConsole().OnScriptEngineStarted();

    Runtime::GetLogger().AddEx_WithoutOutput( _T("; CScriptEngine::Run - start (instance = %s)"), GetInstanceStr() );

    tstr S;
    bool isNppExec = false;
    bool isFirstCmd = true;

    m_execState.nExecCounter = 0;
    m_execState.nExecMaxCount = m_pNppExec->GetOptions().GetInt(OPTI_EXEC_MAXCOUNT);
    m_execState.nGoToCounter = 0;
    m_execState.nGoToMaxCount = m_pNppExec->GetOptions().GetInt(OPTI_GOTO_MAXCOUNT);
    m_execState.pScriptLineCurrent = NULL;
    m_execState.SetScriptLineNext(INVALID_TSTR_LIST_ITEM);
    m_execState.pChildProcess.reset();

    m_execState.ScriptContextList.Add( ScriptContext() );
    {
        ScriptContext& currentScript = m_execState.GetCurrentScriptContext();
        currentScript.CmdRange.pBegin = m_CmdList.GetFirst();
        currentScript.CmdRange.pEnd = 0;
        currentScript.IsNppExeced = false;
        if ( m_nRunFlags & rfShareLocalVars )
        {
            if ( m_pParentScriptEngine )
            {
                // inheriting parent script's local variables
                CCriticalSectionLockGuard lock(m_pNppExec->GetMacroVars().GetCsUserMacroVars());
                currentScript.LocalMacroVars = m_pParentScriptEngine->GetExecState().GetCurrentScriptContext().LocalMacroVars;
                // We could use swap() instead of copying here, but if something
                // goes wrong in that case we risk to end with empty local vars
                // in the parent script. So copying is safer.
            }
        }
        else if ( m_nRunFlags & rfConsoleLocalVarsRead )
        {
            // inheriting Console's local variables
            CCriticalSectionLockGuard lock(m_pNppExec->GetMacroVars().GetCsUserMacroVars());
            currentScript.LocalMacroVars = m_pNppExec->GetMacroVars().GetUserConsoleMacroVars();
        }
        if ( ((m_nRunFlags & rfShareConsoleState) != 0) || 
             (!m_pParentScriptEngine) || (!m_pParentScriptEngine->IsCollateral()) )
        {
            // inheriting Console's enabled state
            CNppExecConsole& Console = m_pNppExec->GetConsole();
            DWORD scrptEngnId = 0;
            if ( ((m_nRunFlags & rfConsoleLocalVars) == 0) && m_pParentScriptEngine )
                scrptEngnId = m_pParentScriptEngine->GetThreadId();
            int nEnabled = Console.GetOutputEnabledDirectly(scrptEngnId);
            Console.SetOutputEnabledDirectly(m_dwThreadId, nEnabled);
        }
    }

    CListItemT<tstr>* p = m_CmdList.GetFirst();
    while ( p && ContinueExecution() )
    {
        eCmdType nCmdType = CMDTYPE_COMMENT_OR_EMPTY;

        m_execState.pScriptLineCurrent = p;
        m_execState.SetScriptLineNext(INVALID_TSTR_LIST_ITEM);

        S = p->GetItem();

        if ( S.length() > 0 )
        {

            if ( (m_nRunFlags & rfCollateralScript) == 0 )
            {
                Runtime::GetLogger().Clear();
            }
            Runtime::GetLogger().Add(   _T("; command info") );
            Runtime::GetLogger().AddEx( _T("Current command item:  p = 0x%X"), p );
            Runtime::GetLogger().Add(   _T("{") );
            Runtime::GetLogger().IncIndentLevel();
            Runtime::GetLogger().AddEx( _T("_item  = \"%s\""), p->GetItem().c_str() );
            Runtime::GetLogger().AddEx( _T("_prev  = 0x%X"), p->GetPrev() );
            if ( p->GetPrev() )
            {
                const CListItemT<tstr>* pPrev = p->GetPrev();
                Runtime::GetLogger().Add(   _T("{") );
                Runtime::GetLogger().IncIndentLevel();
                Runtime::GetLogger().AddEx( _T("_item = \"%s\""), pPrev->GetItem().c_str() );
                Runtime::GetLogger().AddEx( _T("_prev = 0x%X;  _next = 0x%X;  _owner = 0x%X"), 
                    pPrev->GetPrev(), pPrev->GetNext(), pPrev->GetOwner() );
                Runtime::GetLogger().DecIndentLevel();
                Runtime::GetLogger().Add(   _T("}") );
            }
            Runtime::GetLogger().AddEx( _T("_next  = 0x%X"), p->GetNext() ); 
            if ( p->GetNext() )
            {
                const CListItemT<tstr>* pNext = p->GetNext();
                Runtime::GetLogger().Add(   _T("{") );
                Runtime::GetLogger().IncIndentLevel();
                Runtime::GetLogger().AddEx( _T("_item = \"%s\""), pNext->GetItem().c_str() );
                Runtime::GetLogger().AddEx( _T("_prev = 0x%X;  _next = 0x%X;  _owner = 0x%X"), 
                    pNext->GetPrev(), pNext->GetNext(), pNext->GetOwner() );
                Runtime::GetLogger().DecIndentLevel();
                Runtime::GetLogger().Add(   _T("}") );
            }
            Runtime::GetLogger().AddEx( _T("_owner = 0x%X"), p->GetOwner() );
            Runtime::GetLogger().DecIndentLevel();
            Runtime::GetLogger().Add(   _T("}") );
        
            Runtime::GetLogger().Add(   _T("; executing ModifyCommandLine") );

            ScriptContext& currentScript = m_execState.GetCurrentScriptContext();
            const int ifDepth = currentScript.GetIfDepth();
            const eIfState ifState = currentScript.GetIfState();

            nCmdType = modifyCommandLine(this, S, ifState);
            if ( nCmdType != CMDTYPE_COMMENT_OR_EMPTY )
            {
                if ( isSkippingThisCommandDueToIfState(nCmdType, ifState) )
                {
                    Runtime::GetLogger().AddEx( _T("; skipping - waiting for %s"),
                        (ifState == IF_WANT_ENDIF || ifState == IF_WANT_SILENT_ENDIF || ifState == IF_EXECUTING) ? DoEndIfCommand::Name() : DoElseCommand::Name() );
                
                    if ( nCmdType == CMDTYPE_IF )
                    {
                        // nested IF inside a conditional block that is being skipped
                        currentScript.PushIfState(IF_WANT_SILENT_ENDIF);
                        // skip the nested IF...ENDIF block as well - mark it as "silent"
                    }
                    else if ( nCmdType == CMDTYPE_ENDIF )
                    {
                        // nested ENDIF inside a conditional block that is being skipped
                        if ( ifState == IF_WANT_SILENT_ENDIF ) // <-- this condition is redundant, but let it be just in case
                        {
                            // nested "silent" IF...ENDIF block is completed
                            currentScript.PopIfState();
                        }
                    }
                    else if ( nCmdType == CMDTYPE_ELSE )
                    {
                        if ( ifState == IF_EXECUTING )
                        {
                            // the IF...ELSE block completed
                            currentScript.SetIfState(IF_WANT_ENDIF);
                        }
                    }
                }
                else
                {
                    eCmdResult nCmdResult = CMDRESULT_SUCCEEDED;

                    if ( nCmdType == CMDTYPE_COLLATERAL_FORCED )
                    {
                        CListT<tstr> CmdList(S);
                        if ( !m_pNppExec->GetCommandExecutor().ExecuteCollateralScript(CmdList, tstr()) )
                            nCmdResult = CMDRESULT_FAILED;
                    }
                    else
                    {
                        m_nCmdType = nCmdType;
                        if ( isFirstCmd && (m_nCmdType == CMDTYPE_NPPEXEC) )
                        {
                            isNppExec = true;
                        }
                        else 
                        {
                            if ( isFirstCmd )
                            {
                                isFirstCmd = false;

                                if ( m_nCmdType == CMDTYPE_NPPCONSOLE )
                                {
                                    int param = getOnOffParam(S);
                                    if ( param == PARAM_ENABLE || param == PARAM_DISABLE )
                                    {
                                        if ( !m_pNppExec->isConsoleDialogVisible() )
                                        {
                                            m_pNppExec->showConsoleDialog(CNppExec::showIfHidden, 0);
                                        }
                                    }
                                }
                            }
                            if ( isNppExec )
                            {
                                if ( m_nCmdType != CMDTYPE_NPPCONSOLE )
                                {
                                    m_pNppExec->showConsoleDialog(CNppExec::showIfHidden, 0);
                                }
                                isNppExec = false;
                            }
                        }

                        m_sCmdParams = S;
                        EXECFUNC pCmdExecFunc = m_CommandRegistry.GetCmdExecFunc(m_nCmdType);
                        nCmdResult = pCmdExecFunc(this, S);
                    }

                    // The same currentScript object is used here to handle NPP_EXEC as well
                    // (it's safe because ScriptContextList.DeleteLast() is not called before)
                    if ( (ifState == IF_MAYBE_ELSE) &&
                         (currentScript.GetIfState() == IF_MAYBE_ELSE) &&
                         (currentScript.GetIfDepth() == ifDepth) )
                    {
                        // The IfState was not changed, so remove IF_MAYBE_ELSE
                        currentScript.SetIfState(IF_NONE);
                    }

                    {
                        TCHAR szCmdResult[50];
                        c_base::_tint2str(nCmdResult, szCmdResult);

                        tstr varName = MACRO_LAST_CMD_RESULT;
                        m_pNppExec->GetMacroVars().SetUserMacroVar( this, varName, szCmdResult, CNppExecMacroVars::svLocalVar ); // local var
                    }
                }
            }

            Runtime::GetLogger().Add(   _T("") );
        }

        p = (m_execState.pScriptLineNext == INVALID_TSTR_LIST_ITEM) ? p->GetNext() : m_execState.pScriptLineNext;

        ScriptContext& currentScript = m_execState.GetCurrentScriptContext();
        if ( currentScript.IsNppExeced )
        {
            if ( p == currentScript.CmdRange.pEnd )
            {
                Runtime::GetLogger().AddEx( _T("; script context removed: { Name = \"%s\"; CmdRange = [0x%X, 0x%X) }"), 
                    currentScript.ScriptName.c_str(), currentScript.CmdRange.pBegin, currentScript.CmdRange.pEnd ); 

                CListItemT<tstr>* pItem = currentScript.CmdRange.pBegin;
                while ( pItem != currentScript.CmdRange.pEnd )
                {
                    CListItemT<tstr>* pNext = pItem->GetNext();
                    m_CmdList.Delete(pItem);
                    pItem = pNext;
                }
                m_execState.ScriptContextList.DeleteLast();

                Runtime::GetLogger().Add(   _T("") );
            }
        }
    } // while

    if ( isNppExec )
    {
        // the whole script contains just NPP_EXEC commands
        // the Console is still hidden, it must be shown
        m_pNppExec->showConsoleDialog(CNppExec::showIfHidden, 0);
    }

    if ( !m_pNppExec->GetCommandExecutor().GetRunningScriptEngine() )
    {
        // a collateral script may be running
        m_pNppExec->_consoleIsVisible = m_pNppExec->isConsoleDialogVisible();
    }

    Runtime::GetLogger().AddEx_WithoutOutput( _T("; CScriptEngine::Run - end (instance = %s)"), GetInstanceStr() );

    if ( m_nRunFlags & rfShareLocalVars )
    {
        if ( m_pParentScriptEngine )
        {
            ScriptContext& currentScript = m_execState.GetCurrentScriptContext();
            CCriticalSectionLockGuard lock(m_pNppExec->GetMacroVars().GetCsUserMacroVars());
            m_pParentScriptEngine->GetExecState().GetCurrentScriptContext().LocalMacroVars.swap(currentScript.LocalMacroVars);
        }
    }
    else if ( m_nRunFlags & rfConsoleLocalVarsWrite )
    {
        ScriptContext& currentScript = m_execState.GetCurrentScriptContext();
        CCriticalSectionLockGuard lock(m_pNppExec->GetMacroVars().GetCsUserMacroVars());
        m_pNppExec->GetMacroVars().GetUserConsoleMacroVars().swap(currentScript.LocalMacroVars);
    }
    if ( m_nRunFlags & rfShareConsoleState ) // <-- another script can enable the output, but usually that does not affect the parent's state
    {
        CNppExecConsole& Console = m_pNppExec->GetConsole();
        int nEnabled = Console.GetOutputEnabledDirectly(m_dwThreadId);
        DWORD scrptEngnId = 0;
        if ( ((m_nRunFlags & rfConsoleLocalVars) == 0) && m_pParentScriptEngine )
            scrptEngnId = m_pParentScriptEngine->GetThreadId();
        Console.SetOutputEnabledDirectly(scrptEngnId, nEnabled);
    }

    m_pNppExec->GetConsole().OnScriptEngineFinished();

    m_eventRunIsDone.Set();
}

bool CScriptEngine::IsParentOf(const std::shared_ptr<CScriptEngine> pScriptEngine) const
{
    std::shared_ptr<CScriptEngine> pEngine = GetChildScriptEngine();
    while ( pEngine )
    {
        if ( pEngine == pScriptEngine )
            return true;

        pEngine = pEngine->GetChildScriptEngine();
    }
    return false;
}

bool CScriptEngine::IsChildOf(const std::shared_ptr<CScriptEngine> pScriptEngine) const
{
    std::shared_ptr<CScriptEngine> pEngine = GetParentScriptEngine();
    while ( pEngine )
    {
        if ( pEngine == pScriptEngine )
            return true;

        pEngine = pEngine->GetParentScriptEngine();
    }
    return false;
}

bool CScriptEngine::ContinueExecution() const
{
    return (m_pNppExec->_consoleIsVisible || ((m_nRunFlags & rfExitScript) != 0 && !m_pNppExec->_bStopTheExitScript)); 
}

void CScriptEngine::ScriptError(eErrorType type, const TCHAR* cszErrorMessage)
{
    switch ( type )
    {
        case ET_REPORT:
            m_pNppExec->GetConsole().PrintError( GetLastLoggedCmd().c_str() );
            m_pNppExec->GetConsole().PrintError( cszErrorMessage );
            break;

        case ET_ABORT:
          #if SCRPTENGNID_DEBUG_OUTPUT
            {
                tstr S;
                S.Format(1020, _T("CScriptEngine::ScriptError - scrptEngnId=%u\n"), m_dwThreadId);
                ::OutputDebugString(S.c_str());
            }
          #endif
            m_execState.pScriptLineNext = NULL; // stop the script
            m_eventAbortTheScript.Set();
            m_pNppExec->GetConsole().OnScriptEngineAborting(m_dwThreadId);
            Runtime::GetLogger().Add_WithoutOutput( cszErrorMessage );
            Runtime::GetLogger().AddEx_WithoutOutput( _T("; CScriptEngine::ScriptError(ET_ABORT) (instance = %s)"), GetInstanceStr() );
            break;

        case ET_UNPREDICTABLE:
            m_execState.pScriptLineNext = NULL; // stop the script
            m_eventAbortTheScript.Set();
            // do not call it here: m_pNppExec->GetConsole().OnScriptEngineAborting(m_dwThreadId);
            m_pNppExec->GetConsole().PrintError( GetLastLoggedCmd().c_str() );
            m_pNppExec->GetConsole().PrintError( cszErrorMessage );
            m_pNppExec->GetConsole().PrintError( _T("To prevent unpredictable behavior, the script is stopped.") );
            Runtime::GetLogger().AddEx_WithoutOutput( _T("; CScriptEngine::ScriptError(ET_UNPREDICTABLE)  (instance = %s)"), GetInstanceStr() );
            break;
    }
}

void CScriptEngine::UndoAbort(const TCHAR* cszMessage)
{
    m_execState.pScriptLineNext = m_execState.pScriptLineNextBackupCopy;
    m_eventAbortTheScript.Reset();

    Runtime::GetLogger().AddEx_WithoutOutput( _T("; CScriptEngine::UndoAbort() (instance = %s) : %s"), GetInstanceStr(), cszMessage );
}

std::shared_ptr<CChildProcess> CScriptEngine::GetRunningChildProcess()
{
    std::shared_ptr<CChildProcess> pChildProc;
    std::shared_ptr<CScriptEngine> pScriptEngine = m_pNppExec->GetCommandExecutor().GetRunningScriptEngine(); // shared_ptr(this)
    while ( pScriptEngine && !pChildProc )
    {
        pChildProc = pScriptEngine->GetExecState().GetRunningChildProcess();
        pScriptEngine = pScriptEngine->GetParentScriptEngine();
    }
    return pChildProc;
}

bool CScriptEngine::IsChildProcessRunning() const
{
    bool isChildProcRunning = false;
    std::shared_ptr<CScriptEngine> pScriptEngine = m_pNppExec->GetCommandExecutor().GetRunningScriptEngine(); // shared_ptr(this)
    while ( pScriptEngine && !isChildProcRunning )
    {
        isChildProcRunning = pScriptEngine->GetExecState().IsChildProcessRunning();
        pScriptEngine = pScriptEngine->GetParentScriptEngine();
    }
    return isChildProcRunning;
}

void CScriptEngine::ChildProcessMustBreakAll()
{
    std::shared_ptr<CScriptEngine> pScriptEngine = m_pNppExec->GetCommandExecutor().GetRunningScriptEngine(); // shared_ptr(this)
    while ( pScriptEngine )
    {
        std::shared_ptr<CChildProcess> pChildProc = pScriptEngine->GetExecState().GetRunningChildProcess();
        if ( pChildProc )
            pChildProc->MustBreak(CProcessKiller::killCtrlBreak);
        pScriptEngine = pScriptEngine->GetParentScriptEngine();
    }
}

bool CScriptEngine::WaitUntilDone(DWORD dwTimeoutMs) const
{
    return (m_eventRunIsDone.IsNull() || (m_eventRunIsDone.Wait(dwTimeoutMs) == WAIT_OBJECT_0));
}

CScriptEngine::eNppExecCmdPrefix CScriptEngine::checkNppExecCmdPrefix(CNppExec* pNppExec, tstr& Cmd, bool bRemovePrefix)
{
    eNppExecCmdPrefix ret = CmdPrefixNone;
    int nPrefixLen = 0;
    const TCHAR* pszPrefix = pNppExec->GetOptions().GetStr(OPTS_NPPEXEC_CMD_PREFIX, &nPrefixLen);
    if ( nPrefixLen != 0 )
    {
        if ( Cmd.StartsWith(pszPrefix) )
        {
            ret = CmdPrefixCollateralOrRegular;
            if ( Cmd.GetAt(nPrefixLen) == pszPrefix[nPrefixLen - 1] ) // is the last symbol doubled?
            {
                // if the prefix is "nppexec:" (default), then "nppexec::" is expected here
                // if the prefix was changed to e.g. "npe-", then "npe--" is expected here
                ++nPrefixLen;
                ret = CmdPrefixCollateralForced;
            }
            if ( bRemovePrefix )
            {
                Cmd.Delete(0, nPrefixLen);
                NppExecHelpers::StrDelLeadingTabSpaces(Cmd);
            }
        }
    }
    return ret;
}

CScriptEngine::eCmdType CScriptEngine::getCmdType(CNppExec* pNppExec, tstr& Cmd, unsigned int nFlags)
{
    const bool useLogging = ((nFlags & ctfUseLogging) != 0);
    const bool ignorePrefix = ((nFlags & ctfIgnorePrefix) != 0);
    
    if ( useLogging )
    {
        Runtime::GetLogger().Add(   _T("GetCmdType()") );
        Runtime::GetLogger().Add(   _T("{") );
        Runtime::GetLogger().IncIndentLevel();
        Runtime::GetLogger().AddEx( _T("[in]  \"%s\""), Cmd.c_str() );
    }

    NppExecHelpers::StrDelLeadingTabSpaces(Cmd);
    NppExecHelpers::StrDelTrailingTabSpaces(Cmd);

    CScriptEngine::eNppExecCmdPrefix cmdPrefix = checkNppExecCmdPrefix(pNppExec, Cmd);

    if ( Cmd.IsEmpty() )
    {
        if ( useLogging )
        {
            Runtime::GetLogger().AddEx( _T("[ret] %d (empty command)"), CMDTYPE_COMMENT_OR_EMPTY );
            Runtime::GetLogger().DecIndentLevel();
            Runtime::GetLogger().Add(   _T("}") );
        }

        return CMDTYPE_COMMENT_OR_EMPTY;
    }

    if ( (!ignorePrefix) && (cmdPrefix == CmdPrefixCollateralForced) )
    {
        if ( useLogging )
        {
            Runtime::GetLogger().AddEx( _T("[ret] %d (forced collateral command)"), CMDTYPE_COLLATERAL_FORCED );
            Runtime::GetLogger().DecIndentLevel();
            Runtime::GetLogger().Add(   _T("}") );
        }

        return CMDTYPE_COLLATERAL_FORCED;
    }

    const TCHAR* pAliasNppExec = pNppExec->GetOptions().GetStr(OPTS_ALIAS_CMD_NPPEXEC);
    if ( pAliasNppExec && pAliasNppExec[0] )
    {
        if ( pAliasNppExec[0] == Cmd.GetAt(0) ) // the alias is one character
        {
            if ( (Cmd.GetAt(0) != _T('\\') || Cmd.GetAt(1) != _T('\\')) &&  // not "\\..."
                 (Cmd.GetAt(0) != _T('/') || Cmd.GetAt(1) != _T('/')) )     // not "//..."
            {
                Cmd.Delete(0, 1);

                if ( useLogging )
                {
                    Runtime::GetLogger().AddEx( _T("[ret] 0x%X (%s)"), CMDTYPE_NPPEXEC, DoNppExecCommand::Name() );
                    Runtime::GetLogger().DecIndentLevel();
                    Runtime::GetLogger().Add(   _T("}") );
                }

                return CMDTYPE_NPPEXEC;
            }
        }
    }

    pNppExec->GetMacroVars().CheckCmdAliases(Cmd, useLogging);

    if ( Cmd.GetAt(0) == _T(':') && Cmd.GetAt(1) == _T(':') )
    {
        pNppExec->GetConsole().PrintError( _T("- can not use \"::\" at the beginning of line!") );

        if ( useLogging )
        {
            Runtime::GetLogger().AddEx( _T("[ret] %d (command starts with ::)"), CMDTYPE_COMMENT_OR_EMPTY );
            Runtime::GetLogger().DecIndentLevel();
            Runtime::GetLogger().Add(   _T("}") );
        }

        return CMDTYPE_COMMENT_OR_EMPTY;
    }

    if ( DEFAULT_ALIAS_CMD_LABEL == Cmd.GetAt(0) )
    {
        Cmd.Delete(0, 1);

        if ( useLogging )
        {
            Runtime::GetLogger().AddEx( _T("[ret] 0x%X (%s)"), CMDTYPE_LABEL, DoLabelCommand::Name() );
            Runtime::GetLogger().DecIndentLevel();
            Runtime::GetLogger().Add(   _T("}") );
        }

        return CMDTYPE_LABEL;
    }

    tstr S = Cmd;
    NppExecHelpers::StrUpper(S);
  
    eCmdType nCmdType = CMDTYPE_UNKNOWN;
    if ( S.StartsWith(DoCdCommand::Name()) )
    {
        int i = lstrlen(DoCdCommand::Name());
        const TCHAR next_ch = S.GetAt(i);
        if ( IsTabSpaceOrEmptyChar(next_ch) || next_ch == _T('\\') || next_ch == _T('/') || next_ch == _T('.') )
        {
            nCmdType = DoCdCommand::Type();
            Cmd.Delete(0, i);
        }
    }
    if ( nCmdType == CMDTYPE_UNKNOWN )
    {
        int i = S.FindOneOf(_T(" \t"));
        S.Delete(i);
        nCmdType = m_CommandRegistry.GetCmdTypeByName(S);
        if ( nCmdType != CMDTYPE_UNKNOWN )
            Cmd.Delete(0, i);
    }

    if ( useLogging )
    {
        if ( nCmdType != CMDTYPE_UNKNOWN )
        {
            Runtime::GetLogger().AddEx( _T("[ret] 0x%X (%s)"), nCmdType, m_CommandRegistry.GetCmdNameByType(nCmdType) );
        }
        else
        {
            Runtime::GetLogger().AddEx( _T("[ret] 0x%X (unknown)"), nCmdType );
        }
        Runtime::GetLogger().DecIndentLevel();
        Runtime::GetLogger().Add(   _T("}") );
    }
  
    return nCmdType;
}

bool CScriptEngine::isSkippingThisCommandDueToIfState(eCmdType cmdType, eIfState ifState)
{
    return ( (ifState == IF_WANT_SILENT_ENDIF) ||
             ((ifState == IF_WANT_ENDIF) && (cmdType != CMDTYPE_ENDIF)) ||
             ((ifState == IF_WANT_ELSE) && (cmdType != CMDTYPE_ELSE && cmdType != CMDTYPE_ENDIF)) || 
             ((ifState == IF_EXECUTING) && (cmdType == CMDTYPE_ELSE)) );
}

CScriptEngine::eCmdType CScriptEngine::modifyCommandLine(CScriptEngine* pScriptEngine, tstr& Cmd, eIfState ifState)
{
    Runtime::GetLogger().Add(   _T("ModifyCommandLine()") );
    Runtime::GetLogger().Add(   _T("{") );
    Runtime::GetLogger().IncIndentLevel();
    Runtime::GetLogger().AddEx( _T("[in]  \"%s\""), Cmd.c_str() );

    CNppExec* pNppExec = pScriptEngine->GetNppExec();
    CScriptEngine::eNppExecCmdPrefix cmdPrefix = checkNppExecCmdPrefix(pNppExec, Cmd);

    if ( isCommentOrEmpty(pNppExec, Cmd) )
    {
        Runtime::GetLogger().Add(   _T("; it\'s a comment or empty string") );
        Runtime::GetLogger().Add(   _T("; command argument(s):") );
        Runtime::GetLogger().AddEx( _T("[out] \"%s\""), Cmd.c_str() );
        Runtime::GetLogger().Add(   _T("; command type:") );
        Runtime::GetLogger().AddEx( _T("[ret] %d"), CMDTYPE_COMMENT_OR_EMPTY );
        Runtime::GetLogger().DecIndentLevel();
        Runtime::GetLogger().Add(   _T("}") );
      
        return CMDTYPE_COMMENT_OR_EMPTY;
    }

    if ( cmdPrefix == CmdPrefixCollateralForced )
    {
        Runtime::GetLogger().Add(   _T("; it\'s a forced collateral command") );
        Runtime::GetLogger().Add(   _T("; command type:") );
        Runtime::GetLogger().AddEx( _T("[ret] %d (forced collateral command)"), CMDTYPE_COLLATERAL_FORCED );
        Runtime::GetLogger().DecIndentLevel();
        Runtime::GetLogger().Add(   _T("}") );

        return CMDTYPE_COLLATERAL_FORCED;
    }
  
    // ... checking commands ...

    eCmdType nCmdType = getCmdType(pNppExec, Cmd);
    
    if ( nCmdType == CMDTYPE_COMMENT_OR_EMPTY )
    {
        Runtime::GetLogger().DecIndentLevel();
        Runtime::GetLogger().Add(   _T("}") );

        return nCmdType;
    }

    if ( (nCmdType != CMDTYPE_UNKNOWN) && 
         isSkippingThisCommandDueToIfState(nCmdType, ifState) )
    {
        Runtime::GetLogger().DecIndentLevel();
        Runtime::GetLogger().Add(   _T("}") );

        return nCmdType;
    }

    NppExecHelpers::StrDelLeadingTabSpaces(Cmd);
    if ( Cmd.IsEmpty() || (nCmdType == CMDTYPE_CLS) )
    {
        Runtime::GetLogger().Add(   _T("; no arguments given") );
        Runtime::GetLogger().Add(   _T("; command argument(s):") );
        Runtime::GetLogger().AddEx( _T("[out] \"%s\""), Cmd.c_str() );
        Runtime::GetLogger().Add(   _T("; command type:") );
        Runtime::GetLogger().AddEx( _T("[ret] 0x%X"), nCmdType );
        Runtime::GetLogger().DecIndentLevel();
        Runtime::GetLogger().Add(   _T("}") );
      
        return nCmdType;
    }
  
    if ( (nCmdType != CMDTYPE_SET) &&
         (nCmdType != CMDTYPE_UNSET) &&
         (nCmdType != CMDTYPE_NPPSENDMSG) &&
         (nCmdType != CMDTYPE_NPPSENDMSGEX) &&
         (nCmdType != CMDTYPE_SCISENDMSG) &&
         (nCmdType != CMDTYPE_SCIFIND) &&
         (nCmdType != CMDTYPE_SCIREPLACE) &&
         (nCmdType != CMDTYPE_NPECMDALIAS) &&
         (nCmdType != CMDTYPE_NPEQUEUE) &&
         (nCmdType != CMDTYPE_CONFILTER) )
    {
        bool bCmdStartsWithMacroVar = Cmd.StartsWith(_T("$("));

        CNppExecMacroVars& MacroVars = pNppExec->GetMacroVars();

        // ... checking script's arguments ...
        const CStrSplitT<TCHAR> args;
        MacroVars.CheckCmdArgs(Cmd, args);

        // ... checking all the macro-variables ...
        MacroVars.CheckAllMacroVars(pScriptEngine, Cmd, true);
        
        if ( bCmdStartsWithMacroVar && (nCmdType == CMDTYPE_UNKNOWN) )
        {
            // re-check nCmdType after macro-var substitution
            nCmdType = getCmdType(pNppExec, Cmd);
            if ( nCmdType == CMDTYPE_COLLATERAL_FORCED )
            {
                Runtime::GetLogger().Add(   _T("; it\'s a forced collateral command") );
                Runtime::GetLogger().Add(   _T("; command type:") );
                Runtime::GetLogger().AddEx( _T("[ret] %d (forced collateral command)"), CMDTYPE_COLLATERAL_FORCED );
                Runtime::GetLogger().DecIndentLevel();
                Runtime::GetLogger().Add(   _T("}") );

                return CMDTYPE_COLLATERAL_FORCED;
            }
            
            if ( nCmdType != CMDTYPE_UNKNOWN )
            {
                NppExecHelpers::StrDelLeadingTabSpaces(Cmd);
            }
        }

        if ( isSkippingThisCommandDueToIfState(nCmdType, ifState) )
        {
            Runtime::GetLogger().DecIndentLevel();
            Runtime::GetLogger().Add(   _T("}") );

            return nCmdType;
        }
    }
    // we have to process macro-vars inside doSendMsg()
    // because macro-var's string may contain double-quotes
  
    // ... do we need "" around the command's argument? ...
    
    bool bDone = false;
    if ( (nCmdType == CMDTYPE_SET) ||
         (nCmdType == CMDTYPE_UNSET) ||
         (nCmdType == CMDTYPE_NPECMDALIAS) ||
         (nCmdType == CMDTYPE_NPPSENDMSG) ||
         (nCmdType == CMDTYPE_NPPSENDMSGEX) || 
         (nCmdType == CMDTYPE_SCISENDMSG) ||
         (nCmdType == CMDTYPE_SCIFIND) ||
         (nCmdType == CMDTYPE_SCIREPLACE) ||
         (nCmdType == CMDTYPE_CONCOLOUR) ||
         (nCmdType == CMDTYPE_CONFILTER) ||
         (nCmdType == CMDTYPE_IF) || 
         (nCmdType == CMDTYPE_GOTO) || 
         (nCmdType == CMDTYPE_ELSE) ||
         (nCmdType == CMDTYPE_SLEEP) ||
         (nCmdType == CMDTYPE_NPEQUEUE) )
    {
        bDone = true;
    }
    else if ( (nCmdType != 0) &&
              (nCmdType != CMDTYPE_NPPRUN) &&
              (nCmdType != CMDTYPE_NPPEXEC) &&
              (nCmdType != CMDTYPE_ECHO) &&
              (nCmdType != CMDTYPE_SELSAVETO) &&
              (nCmdType != CMDTYPE_INPUTBOX) )
    {
        NppExecHelpers::StrUnquote(Cmd);

        bDone = true; // we don't need '\"' in file_name
    }

    // ... adding '\"' to the command if it's needed ...
    if ( !bDone )
    {
        bool bHasSpaces = false;
        // disabled by default  because of problems 
        // for executables without extension i.e. 
        // "cmd /c app.exe"  <-- "cmd" is without extension
        if (pNppExec->GetOptions().GetBool(OPTB_PATH_AUTODBLQUOTES))
        {
            if (!bDone && (Cmd.GetAt(0) != _T('\"')))
            {
                int i = 0;
                int j = 0;
                while (!bDone && (i < Cmd.length()))
                {
                    if (Cmd[i] == _T(' '))
                    {
                        bHasSpaces = true;
                        j = i - 1;
                        while (!bDone && j >= 0)
                        {
                            const TCHAR ch = Cmd[j];
                            if (ch == _T('.'))
                            {
                                Cmd.Insert(i, _T('\"'));
                                Cmd.Insert(0, _T('\"'));
                                bDone = true;
                            }
                            else if (ch == _T('\\') || ch == _T('/'))
                            {
                                j = 0; // j-- makes j<0 so this loop is over
                            }
                            j--;
                        }
                    }
                    i++;
                }
            }
        }
    }
    
    Runtime::GetLogger().Add(   _T("; command argument(s):") );
    Runtime::GetLogger().AddEx( _T("[out] \"%s\""), Cmd.c_str() );
    Runtime::GetLogger().Add(   _T("; command type:") );
    Runtime::GetLogger().AddEx( _T("[ret] 0x%X"), nCmdType );
    Runtime::GetLogger().DecIndentLevel();
    Runtime::GetLogger().Add(   _T("}") );

    return nCmdType;
}

int CScriptEngine::getOnOffParam(const tstr& param)
{
    if ( param.IsEmpty() )
        return PARAM_EMPTY; // no param
    
    if ( param == _T("1") )
        return PARAM_ON;

    if ( param == _T("0") )
        return PARAM_OFF;

    if ( param == _T("?") )
        return PARAM_KEEP;

    if ( param == _T("+") )
        return PARAM_ENABLE;

    if ( param == _T("-") )
        return PARAM_DISABLE;

    tstr S = param;
    NppExecHelpers::StrUpper(S);

    if ( (S == _T("ON")) || (S == _T("TRUE")) )
        return PARAM_ON;

    if ( (S == _T("OFF")) || (S == _T("FALSE")) )
        return PARAM_OFF;

    if ( S == _T("KEEP") )
        return PARAM_KEEP;

    if ( S == _T("ENABLE") )
        return PARAM_ENABLE;

    if ( S == _T("DISABLE") )
        return PARAM_DISABLE;

    return PARAM_UNKNOWN; // unknown
}

bool CScriptEngine::isCommentOrEmpty(CNppExec* pNppExec, tstr& Cmd)
{
    const tstr comment = pNppExec->GetOptions().GetStr(OPTS_COMMENTDELIMITER);
    if (comment.length() > 0)
    {
        int i = Cmd.Find(comment.c_str()); // comment
        if ((i >= 0) && 
            ((comment != _T("//")) || (Cmd.GetAt(i-1) != _T(':')))) // skip :// e.g. http://
        {
            Cmd.Delete(i, -1); // delete all after "//"

            Runtime::GetLogger().AddEx( _T("; comment removed: everything after %s"), comment.c_str() );

        }
    }
  
    NppExecHelpers::StrDelLeadingTabSpaces(Cmd);
    NppExecHelpers::StrDelTrailingTabSpaces(Cmd);

    return Cmd.IsEmpty();
}

void CScriptEngine::errorCmdNotEnoughParams(const TCHAR* cszCmd, const TCHAR* cszErrorMessage)
{
    if ( cszErrorMessage )
        Runtime::GetLogger().AddEx( _T("; %s"), cszErrorMessage );
    else
        Runtime::GetLogger().Add(   _T("; argument(s) expected, but none given") );

    tstr Cmd = cszCmd;
    tstr Err = Cmd;
    Err += _T(':');
    m_pNppExec->GetConsole().PrintMessage( Err.c_str(), false );
    NppExecHelpers::StrLower(Cmd);
    Err.Format( 250, _T("- %s; type \"help %s\" for help"), cszErrorMessage ? cszErrorMessage : _T("empty command (no parameters given)"), Cmd.c_str() );
    m_pNppExec->GetConsole().PrintError( Err.c_str() );

    m_nCmdType = CMDTYPE_UNKNOWN;
}

void CScriptEngine::errorCmdNoParam(const TCHAR* cszCmd)
{
    errorCmdNotEnoughParams(cszCmd, NULL);
}

void CScriptEngine::messageConsole(const TCHAR* cszCmd, const TCHAR* cszParams)
{
    tstr S = cszCmd;
    S += _T(": ");
    S += cszParams;
    m_pNppExec->GetConsole().PrintMessage( S.c_str() );
}

bool CScriptEngine::reportCmdAndParams(const TCHAR* cszCmd, const tstr& params, unsigned int uFlags)
{
    if ( params.IsEmpty() )
    {
        if ( uFlags & fReportEmptyParam )
            errorCmdNoParam(cszCmd);

        if ( uFlags & fFailIfEmptyParam )
            return false;
    
        Runtime::GetLogger().AddEx( _T("; executing: %s"), cszCmd );
        m_sLoggedCmd.Format( 1020, _T("; executing: %.960s"), cszCmd );
    }
    else
    {
        Runtime::GetLogger().AddEx( _T("; executing: %s %s"), cszCmd, params.c_str() );
        m_sLoggedCmd.Format( 1020, _T("; executing: %.480s %.480s"), cszCmd, params.c_str() );
    }

    if ( uFlags & fMessageToConsole )
    {
        if ( params.length() > MAX_VAR_LENGTH2SHOW )
        {
            tstr S;

            S.Append( params.c_str(), MAX_VAR_LENGTH2SHOW - 5 );
            S += _T("(...)");
            messageConsole( cszCmd, S.c_str() );
        }
        else
        {
            if ( params.IsEmpty() )
                m_pNppExec->GetConsole().PrintMessage( cszCmd );
            else
                messageConsole( cszCmd, params.c_str() );
        }
    }

    return true;
}

void CScriptEngine::updateFocus()
{
    if ( (!m_pNppExec->m_hFocusedWindowBeforeScriptStarted) ||
         (m_pNppExec->m_hFocusedWindowBeforeScriptStarted == m_pNppExec->GetConsole().GetConsoleWnd()) ||
         (m_pNppExec->m_hFocusedWindowBeforeScriptStarted == m_pNppExec->GetConsole().GetDialogWnd()) ||
         (m_pNppExec->m_hFocusedWindowBeforeScriptStarted == m_pNppExec->GetScintillaHandle()) )
    {
        ::SendMessage( m_pNppExec->GetScintillaHandle(), WM_SETFOCUS, 0, 0 );
        m_pNppExec->m_hFocusedWindowBeforeScriptStarted = m_pNppExec->GetScintillaHandle();
    }
}

CScriptEngine::eCmdResult CScriptEngine::Do(const tstr& params)
{
    if ( params.IsEmpty() )
    {
        return CMDRESULT_INVALIDPARAM;
    }

    eCmdResult nCmdResult = CMDRESULT_SUCCEEDED;

    Runtime::GetLogger().AddEx( _T("; about to start a child process: \"%s\""), params.c_str() );
    m_sLoggedCmd.Format( 1020, _T("; about to start a child process: \"%.960s\""), params.c_str() );

    m_pNppExec->GetConsole().PrintMessage( params.c_str() );

    std::shared_ptr<CChildProcess> proc(new CChildProcess(this));
    m_execState.pChildProcess = proc;
    // Note: proc->Create() does not return until the child process exits
    if ( proc->Create(m_pNppExec->GetConsole().GetDialogWnd(), params.c_str()) )
    {
        Runtime::GetLogger().Add(   _T("; child process finished") );
    }
    else
    {
        Runtime::GetLogger().Add(   _T("; failed to start a child process") );

        nCmdResult = CMDRESULT_FAILED;
    }

    TCHAR szExitCode[50];
    c_base::_tint2str(proc->GetExitCode(), szExitCode);

    tstr varName = MACRO_EXITCODE;
    m_pNppExec->GetMacroVars().SetUserMacroVar( this, varName, szExitCode, CNppExecMacroVars::svLocalVar ); // local var

    if ( m_pNppExec->GetOptions().GetBool(OPTB_CONSOLE_SETOUTPUTVAR) )
    {
        tstr& OutputVar = proc->GetOutput();
        if ( OutputVar.GetLastChar() == _T('\n') )
            OutputVar.SetSize(OutputVar.length() - 1);
        if ( OutputVar.GetFirstChar() == _T('\n') )
            OutputVar.Delete(0, 1);
        
        // $(OUTPUT)
        varName = MACRO_OUTPUT;
        m_pNppExec->GetMacroVars().SetUserMacroVar( this, varName, OutputVar, CNppExecMacroVars::svLocalVar ); // local var

        // $(OUTPUTL)
        varName = MACRO_OUTPUTL;

        int i = OutputVar.RFind( _T('\n') );
        if ( i >= 0 )
        {
            tstr varValue;

            ++i;
            varValue.Copy( OutputVar.c_str() + i, OutputVar.length() - i );
            m_pNppExec->GetMacroVars().SetUserMacroVar( this, varName, varValue, CNppExecMacroVars::svLocalVar ); // local var
        }
        else
            m_pNppExec->GetMacroVars().SetUserMacroVar( this, varName, OutputVar, CNppExecMacroVars::svLocalVar ); // local var

        // $(OUTPUT1)
        varName = MACRO_OUTPUT1;

        i = OutputVar.Find( _T('\n') );
        if ( i >= 0 )
        {
            tstr varValue;

            varValue.Copy( OutputVar.c_str(), i );
            m_pNppExec->GetMacroVars().SetUserMacroVar( this, varName, varValue, CNppExecMacroVars::svLocalVar ); // local var
        }
        else
            m_pNppExec->GetMacroVars().SetUserMacroVar( this, varName, OutputVar, CNppExecMacroVars::svLocalVar ); // local var
    }

    m_execState.pChildProcess.reset();

    return nCmdResult;
}

CScriptEngine::eCmdResult CScriptEngine::DoCd(const tstr& params)
{
    eCmdResult nCmdResult = CMDRESULT_SUCCEEDED;
    TCHAR szPath[FILEPATH_BUFSIZE];

    reportCmdAndParams( DoCdCommand::Name(), params, fMessageToConsole );

    // changing current directory

    if ( params.length() > 0 )
    {
        if ( ((params[0] == _T('\\')) && (params.GetAt(1) != _T('\\'))) || 
            ((params[0] == _T('/')) && (params.GetAt(1) != _T('/'))) )
        {
            // root directory of current drive e.g. 'C:\', 'D:\' etc.
            GetCurrentDirectory( FILEPATH_BUFSIZE - 1, szPath );
            if ( szPath[1] == _T(':') )
            {
                szPath[2] = _T('\\');
                szPath[3] = 0;
              
                Runtime::GetLogger().AddEx( _T("; changed to \"%s%s\""), szPath, params.c_str() + 1 );
              
                nCmdResult = SetCurrentDirectory( szPath ) ? CMDRESULT_SUCCEEDED : CMDRESULT_FAILED;
                if ( params[1] )
                {
                    nCmdResult = SetCurrentDirectory( params.c_str() + 1 ) ? CMDRESULT_SUCCEEDED : CMDRESULT_FAILED;
                }
            }
        }
        else
        {
            int ofs = 0;

            if ( (params[1] == _T(':')) && 
                 (params[2] == _T('\\') || params[2] == _T('/')) && 
                 (params[3] != 0) )
            {
                // changing the drive
                szPath[0] = params[0];
                szPath[1] = params[1];
                szPath[2] = 0;
                nCmdResult = SetCurrentDirectory( szPath ) ? CMDRESULT_SUCCEEDED : CMDRESULT_FAILED;
                ofs = 2;
            }
            if ( nCmdResult == CMDRESULT_SUCCEEDED ) // the same drive or successfully changed
            {
                // set current directory
                nCmdResult = SetCurrentDirectory( params.c_str() + ofs ) ? CMDRESULT_SUCCEEDED : CMDRESULT_FAILED;
                if ( nCmdResult == CMDRESULT_FAILED )
                {
                    // trying to set the parent directory at least...
                    // (and nCmdResult is still CMDRESULT_FAILED)
                    for ( int i = params.length() - 1; i > ofs; i-- )
                    {
                        if ( params[i] == _T('\\') || params[i] == _T('/') )
                        {
                            lstrcpyn( szPath, params.c_str() + ofs, i - ofs + 1 );
                            szPath[i - ofs + 1] = 0;
                            SetCurrentDirectory( szPath );
                            break;
                        }
                    }
                }
            }
        }
    }
    GetCurrentDirectory( FILEPATH_BUFSIZE - 1, szPath );
    tstr S = _T("Current directory: ");
    S += szPath;
    m_pNppExec->GetConsole().PrintMessage( S.c_str(), false );

    return nCmdResult;
}

CScriptEngine::eCmdResult CScriptEngine::DoCls(const tstr& /*params*/)
{
    m_pNppExec->GetConsole().ClearText();

    return CMDRESULT_SUCCEEDED;
}

static BOOL getColorFromStr(const TCHAR* szColor, COLORREF* pColor)
{
    if ( szColor && *szColor )
    {
        c_base::byte_t bt[4];

        int n = c_base::_thexstr2buf( szColor, bt, 4 );
        if ( n >= 3 )
        {
            *pColor = RGB( bt[0], bt[1], bt[2] );
            return TRUE;
        }

        if ( n <= 1 )
        {
            if ( szColor[0] == _T('0') && szColor[1] == 0 )
            {
                *pColor = 0; // handle "0" as correct default value
                return TRUE;
            }
        }
    }

    return FALSE;
}

CScriptEngine::eCmdResult CScriptEngine::DoConColour(const tstr& params)
{
    reportCmdAndParams( DoConColourCommand::Name(), params, fMessageToConsole );

    eCmdResult nCmdResult = CMDRESULT_SUCCEEDED;

    if ( params.length() > 0 )
    {
        tstr colorParams = params;
        NppExecHelpers::StrUpper(colorParams);

        const int posFG = colorParams.Find(_T("FG"));
        const int posBG = colorParams.Find(_T("BG"));

        if ( posFG >= 0 )
        {
            tstr colorFG;
            int n = (posBG > posFG) ? (posBG - posFG) : (colorParams.length() - posFG);
            
            colorFG.Append(colorParams.c_str() + posFG, n);
            n = colorFG.Find(_T('='));
            if ( n >= 0 )
            {
                COLORREF color;

                colorFG.Delete(0, n + 1);
                NppExecHelpers::StrDelLeadingTabSpaces(colorFG);
                NppExecHelpers::StrDelTrailingTabSpaces(colorFG);
                if ( (!colorFG.IsEmpty()) && getColorFromStr(colorFG.c_str(), &color) )
                {
                    m_pNppExec->GetConsole().SetCurrentColorTextNorm(color);
                }
                else
                {
                    ScriptError( ET_REPORT, _T("- incorrect value of \'FG\'") );
                    nCmdResult = CMDRESULT_INVALIDPARAM;
                }
            }
            else
            {
                ScriptError( ET_REPORT, _T("- \'FG\' found, but no value specified") );
                nCmdResult = CMDRESULT_INVALIDPARAM;
            }
        }

        if ( posBG >= 0 )
        {
            tstr colorBG;
            int n = (posFG > posBG) ? (posFG - posBG) : (colorParams.length() - posBG);

            colorBG.Append(colorParams.c_str() + posBG, n);
            n = colorBG.Find(_T('='));
            if ( n >= 0 )
            {
                COLORREF color;

                colorBG.Delete(0, n + 1);
                NppExecHelpers::StrDelLeadingTabSpaces(colorBG);
                NppExecHelpers::StrDelTrailingTabSpaces(colorBG);
                if ( (!colorBG.IsEmpty()) && getColorFromStr(colorBG.c_str(), &color) )
                {
                    m_pNppExec->GetConsole().SetCurrentColorBkgnd(color);
                    m_pNppExec->GetConsole().UpdateColours();
                }
                else
                {
                    ScriptError( ET_REPORT, _T("- incorrect value of \'BG\'") );
                    nCmdResult = CMDRESULT_INVALIDPARAM;
                }
            }
            else
            {
                ScriptError( ET_REPORT, _T("- \'BG\' found, but no value specified") );
                nCmdResult = CMDRESULT_INVALIDPARAM;
            }
        }

        if ( (posFG < 0) && (posBG < 0) )
        {
            tstr Err = _T("- unknown parameter: ");
            Err += params;
            ScriptError( ET_REPORT, Err.c_str() );
            nCmdResult = CMDRESULT_INVALIDPARAM;
        }
    }
    else
    {
        tstr S;
        TCHAR buf[16];
        COLORREF color;

        color = m_pNppExec->GetConsole().GetCurrentColorTextNorm();
        buf[0] = 0;
        c_base::_tbuf2hexstr( (const c_base::byte_t*) &color, 3, buf, 16, _T(" ") );
        S = _T("Foreground (text) colour:  ");
        S += buf;
        m_pNppExec->GetConsole().PrintMessage( S.c_str(), false );

        color = m_pNppExec->GetConsole().GetCurrentColorBkgnd();
        buf[0] = 0;
        c_base::_tbuf2hexstr( (const c_base::byte_t*) &color, 3, buf, 16, _T(" ") );
        S = _T("Background colour:         ");
        S += buf;
        m_pNppExec->GetConsole().PrintMessage( S.c_str(), false );
    }

    return nCmdResult;
}

CScriptEngine::eCmdResult CScriptEngine::DoConFilter(const tstr& params)
{
    if ( !reportCmdAndParams( DoConFilterCommand::Name(), params, fMessageToConsole | fReportEmptyParam | fFailIfEmptyParam ) )
        return CMDRESULT_INVALIDPARAM;

    // +(-)i1..5    Include
    // +(-)x1..5    eXclude
    // +(-)fr1..4   Find+Replace (case-insensitive)
    // +(-)frc1..4  Find+Replace (match case)
    // +(-)h1..10   Highlight

    enum eOptionState {
        stateUnknown = 0,
        stateOn,
        stateOff
    };

    enum eOptionType {
        typeUnknown = 0,
        typeInclude,
        typeExclude,
        typeFindReplaceIgnoreCase,
        typeFindReplaceMatchCase,
        typeHighlight
    };

    eCmdResult nCmdResult = CMDRESULT_SUCCEEDED;
    bool isHighlightChanged = false;

    tstr sOption;
    CStrSplitT<TCHAR> args;
    const int n = args.SplitToArgs(params);
    for ( int i = 0; i < n; i++ )
    {
        sOption = args.GetArg(i);
        NppExecHelpers::StrUpper(sOption);

        bool bWrongIndex = false;
        eOptionType type = typeUnknown;
        eOptionState state = stateUnknown;
        if ( sOption.GetAt(0) == _T('+') )
            state = stateOn;
        else if ( sOption.GetAt(0) == _T('-') )
            state = stateOff;

        if ( state != stateUnknown )
        {
            switch ( sOption.GetAt(1) )
            {
                case _T('I'):
                    type = typeInclude;
                    break;
                case _T('X'):
                    type = typeExclude;
                    break;
                case _T('F'):
                    if ( sOption.GetAt(2) == _T('R') )
                    {
                        if ( sOption.GetAt(3) == _T('C') )
                            type = typeFindReplaceMatchCase;
                        else
                            type = typeFindReplaceIgnoreCase;
                    }
                    break;
                case _T('H'):
                    type = typeHighlight;
                    break;
            }

            if ( type != typeUnknown )
            {
                const TCHAR* pIndex = sOption.c_str();
                if ( type == typeFindReplaceMatchCase )
                    pIndex += 4; // skip "+FRC"
                else if ( type == typeFindReplaceIgnoreCase )
                    pIndex += 3; // skip "+FR"
                else
                    pIndex += 2; // skip "+I" or "+X" or "+H"

                const int nIndex = c_base::_tstr2int(pIndex) - 1;
                const bool bEnable = (state == stateOn) ? true : false;
                switch ( type )
                {
                    case typeInclude:
                        if ( nIndex >= 0 && nIndex < CConsoleOutputFilterDlg::FILTER_ITEMS )
                        {
                            const int nIndexMask = (0x01 << nIndex);
                            int nConFltrInclMask = m_pNppExec->GetOptions().GetInt(OPTI_CONFLTR_INCLMASK);
                            nConFltrInclMask |= nIndexMask;
                            if ( !bEnable )  nConFltrInclMask ^= nIndexMask;
                            m_pNppExec->GetOptions().SetInt(OPTI_CONFLTR_INCLMASK, nConFltrInclMask);
                            if ( bEnable )  m_pNppExec->GetOptions().SetBool(OPTB_CONFLTR_ENABLE, true);
                        }
                        else
                        {
                            bWrongIndex = true;
                        }
                        break;
                    case typeExclude:
                        if ( nIndex >= 0 && nIndex < CConsoleOutputFilterDlg::FILTER_ITEMS )
                        {
                            const int nIndexMask = (0x01 << nIndex);
                            int nConFltrExclMask = m_pNppExec->GetOptions().GetInt(OPTI_CONFLTR_EXCLMASK);
                            nConFltrExclMask |= nIndexMask;
                            if ( !bEnable )  nConFltrExclMask ^= nIndexMask;
                            m_pNppExec->GetOptions().SetInt(OPTI_CONFLTR_EXCLMASK, nConFltrExclMask);
                            if ( bEnable )  m_pNppExec->GetOptions().SetBool(OPTB_CONFLTR_ENABLE, true);
                        }
                        else
                        {
                            bWrongIndex = true;
                        }
                        break;
                    case typeFindReplaceIgnoreCase:
                    case typeFindReplaceMatchCase:
                        if ( nIndex >= 0 && nIndex < CConsoleOutputFilterDlg::REPLACE_ITEMS )
                        {
                            const int nIndexMask = (0x01 << nIndex);
                            const bool bMatchCase = (type == typeFindReplaceMatchCase) ? true : false;
                            int nRplcFltrFindMask = m_pNppExec->GetOptions().GetInt(OPTI_CONFLTR_R_FINDMASK);
                            nRplcFltrFindMask |= nIndexMask;
                            if ( !bEnable )  nRplcFltrFindMask ^= nIndexMask;
                            m_pNppExec->GetOptions().SetInt(OPTI_CONFLTR_R_FINDMASK, nRplcFltrFindMask);
                            int nRplcFltrCaseMask = m_pNppExec->GetOptions().GetInt(OPTI_CONFLTR_R_CASEMASK);
                            nRplcFltrCaseMask |= nIndexMask;
                            if ( !bMatchCase )  nRplcFltrCaseMask ^= nIndexMask;
                            m_pNppExec->GetOptions().SetInt(OPTI_CONFLTR_R_CASEMASK, nRplcFltrCaseMask);
                            if ( bEnable )  m_pNppExec->GetOptions().SetBool(OPTB_CONFLTR_R_ENABLE, true);
                        }
                        else
                        {
                            bWrongIndex = true;
                        }
                        break;
                    case typeHighlight:
                        if ( nIndex >= 0 && nIndex < CConsoleOutputFilterDlg::RECOGNITION_ITEMS )
                        {
                            m_pNppExec->GetWarningAnalyzer().EnableEffect(nIndex, bEnable);
                            isHighlightChanged = true;
                        }
                        else
                        {
                            bWrongIndex = true;
                        }
                        break;
                }
            }
        }
        
        if ( state == stateUnknown || type == typeUnknown )
        {
            tstr Err = _T("- unknown option: ");
            Err += sOption;
            ScriptError( ET_REPORT, Err.c_str() );
            nCmdResult = CMDRESULT_INVALIDPARAM;
        }
        else if ( bWrongIndex )
        {
            tstr Err = _T("- wrong index: ");
            Err += sOption;
            ScriptError( ET_REPORT, Err.c_str() );
            nCmdResult = CMDRESULT_INVALIDPARAM;
        }
    }

    if ( isHighlightChanged )
    {
        m_pNppExec->UpdateGoToErrorMenuItem();
    }

    return nCmdResult;
}

CScriptEngine::eCmdResult CScriptEngine::DoConLoadFrom(const tstr& params)
{
    if ( !reportCmdAndParams( DoConLoadFromCommand::Name(), params, fMessageToConsole | fReportEmptyParam | fFailIfEmptyParam ) )
        return CMDRESULT_INVALIDPARAM;

    eCmdResult nCmdResult = CMDRESULT_SUCCEEDED;
    int nBytes = m_pNppExec->conLoadFrom(params.c_str());
    if ( nBytes < 0 )
    {
        ScriptError( ET_REPORT, _T("- can not open the file") );
        nCmdResult = CMDRESULT_FAILED;
    }

    return nCmdResult;
}

CScriptEngine::eCmdResult CScriptEngine::DoConSaveTo(const tstr& params)
{
    if ( !reportCmdAndParams( DoConSaveToCommand::Name(), params, fMessageToConsole | fReportEmptyParam | fFailIfEmptyParam ) )
        return CMDRESULT_INVALIDPARAM;
        
    eCmdResult nCmdResult = CMDRESULT_SUCCEEDED;
    tstr S;
    int  nBytes = m_pNppExec->conSaveTo(params.c_str());
    if ( nBytes >= 0 )
      S.Format(120, _T("- OK, %d bytes have been written to \""), nBytes);
    else
      S = _T("- failed to write to \"");
    S += params;
    S += _T("\"");
    if ( nBytes >= 0 )
    {
        m_pNppExec->GetConsole().PrintMessage( S.c_str() );
    }
    else
    {
        ScriptError( ET_REPORT, S.c_str() );
        nCmdResult = CMDRESULT_FAILED;
    }

    return nCmdResult;
}

CScriptEngine::eCmdResult CScriptEngine::DoDir(const tstr& params)
{
    const TCHAR* cszPathAndFilter = params.IsEmpty() ? _T("*") : params.c_str();
    reportCmdAndParams( DoDirCommand::Name(), cszPathAndFilter, fMessageToConsole );

    tstr Path;
    tstr Filter;
    int  nFilterPos = FileFilterPos( cszPathAndFilter );
    
    GetPathAndFilter( cszPathAndFilter, nFilterPos, Path, Filter );

    Runtime::GetLogger().AddEx( _T("; searching \"%s\" for \"%s\""), Path.c_str(), Filter.c_str() );

    return ( PrintDirContent(m_pNppExec, Path.c_str(), Filter.c_str()) ? CMDRESULT_SUCCEEDED : CMDRESULT_FAILED );
}

CScriptEngine::eCmdResult CScriptEngine::DoEcho(const tstr& params)
{
    reportCmdAndParams( DoEchoCommand::Name(), params, 0 );
     
    m_pNppExec->GetConsole().PrintMessage( params.c_str(), false );

    return CMDRESULT_SUCCEEDED;
}

CScriptEngine::eCmdResult CScriptEngine::DoElse(const tstr& params)
{
    ScriptContext& currentScript = m_execState.GetCurrentScriptContext();
    const eIfState ifState = currentScript.GetIfState();

    eCmdResult nCmdResult = CMDRESULT_SUCCEEDED;
    unsigned int uFlags = 0;
    if ( ifState == IF_NONE || ifState == IF_WANT_ELSE || ifState == IF_MAYBE_ELSE )
        uFlags |= fMessageToConsole;
    reportCmdAndParams( DoElseCommand::Name(), params, uFlags );
    
    if ( ifState == IF_NONE )
    {
        ScriptError( ET_UNPREDICTABLE, _T("- Unexpected ELSE found, without preceding IF.") );
        nCmdResult = CMDRESULT_FAILED;
    }
    else if ( ifState == IF_EXECUTING )
    {
        Runtime::GetLogger().Add(   _T("; IF ... ELSE found, skipping everything up to ENDIF") );

        currentScript.SetIfState(IF_WANT_ENDIF);
    }
    else if ( ifState == IF_WANT_ELSE || ifState == IF_MAYBE_ELSE )
    {
        Runtime::GetLogger().Add( _T("; ELSE found") );

        if ( params.IsEmpty() )
        {
            // ELSE without condition
            currentScript.SetIfState(IF_EXECUTING);
        }
        else
        {
            // ELSE with condition
            tstr paramsUpperCase = params;
            NppExecHelpers::StrUpper(paramsUpperCase);

            static const TCHAR* arrIf[] = {
                _T("IF "),
                _T("IF\t"),
            };

            int n = -1;
            for ( const TCHAR* const& i : arrIf )
            {
                n = paramsUpperCase.RFind(i);
                if ( n >= 0 )
                    break;
            }

            if ( n < 0 )
            {
                ScriptError( ET_UNPREDICTABLE, _T("- ELSE IF expected, but IF was not found.") );
                nCmdResult = CMDRESULT_FAILED;
            }
            else
            {
                tstr ifParams;
                ifParams.Copy( params.c_str() + n + 3 );
                NppExecHelpers::StrDelLeadingTabSpaces(ifParams);
                NppExecHelpers::StrDelTrailingTabSpaces(ifParams);

                doIf(ifParams, true);
            }
        }
    }

    return nCmdResult;
}

CScriptEngine::eCmdResult CScriptEngine::DoEndIf(const tstr& params)
{
    reportCmdAndParams( DoEndIfCommand::Name(), params, fMessageToConsole );

    eCmdResult nCmdResult = CMDRESULT_SUCCEEDED;

    if ( !params.IsEmpty() )
    {
        ScriptError( ET_REPORT, _T("- unexpected parameter(s)") );
        nCmdResult = CMDRESULT_INVALIDPARAM;
    }

    ScriptContext& currentScript = m_execState.GetCurrentScriptContext();
    const eIfState ifState = currentScript.GetIfState();
    if ( ifState == IF_NONE )
    {
        ScriptError( ET_UNPREDICTABLE, _T("- Unexpected ENDIF found, without preceding IF.") );
        nCmdResult = CMDRESULT_FAILED;
    }
    else
    {

        Runtime::GetLogger().Add( _T("; IF ... ENDIF found, done") );

        currentScript.PopIfState();
    }

    return nCmdResult;
}

CScriptEngine::eCmdResult CScriptEngine::DoEnvSet(const tstr& params)
{
    if ( !reportCmdAndParams( DoEnvSetCommand::Name(), params, fMessageToConsole | fReportEmptyParam | fFailIfEmptyParam ) )
        return CMDRESULT_INVALIDPARAM;

    eCmdResult nCmdResult = CMDRESULT_SUCCEEDED;
    bool isInternal = false;
    
    CStrSplitT<TCHAR> args;
    if ( args.Split(params, _T("="), 2) == 2 )
    {
        isInternal = true;
        
        // set the value
        tstr& varName = args.Arg(0);
        tstr& varValue = args.Arg(1);
        
        NppExecHelpers::StrDelLeadingTabSpaces(varName);
        NppExecHelpers::StrDelTrailingTabSpaces(varName);
        NppExecHelpers::StrDelLeadingTabSpaces(varValue);
        NppExecHelpers::StrDelTrailingTabSpaces(varValue);

        if ( varName.length() > 0 )
        {
            NppExecHelpers::StrUpper(varName);

            {
                CCriticalSectionLockGuard lock(g_csEnvVars);
                if ( g_LocalEnvVarNames.find(varName) == g_LocalEnvVarNames.end() )
                {
                    // not Local Env Var; maybe Global?
                    tEnvVars::const_iterator itrGVar = g_GlobalEnvVars.find(varName);
                    if ( itrGVar == g_GlobalEnvVars.end() )
                    {
                        // not in the Global Env Vars List; but maybe it's Global?
                        DWORD nLen = GetEnvironmentVariable(varName.c_str(), NULL, 0);
                        if ( nLen > 0 )
                        {
                            // is Global Env Var
                            TCHAR* pStr = new TCHAR[nLen + 2];
                            if ( pStr )
                            {
                                nLen = GetEnvironmentVariable(varName.c_str(), pStr, nLen + 1);
                                if (nLen > 0)
                                {
                                    g_GlobalEnvVars[varName] = pStr;
                                    itrGVar = g_GlobalEnvVars.find(varName);
                                }
                                delete [] pStr;
                            }
                        }
                        if ( itrGVar == g_GlobalEnvVars.end() )
                        {
                            // not Global Env Var; add to the Local
                            g_LocalEnvVarNames[varName] = 0;
                        }
                    }
                }
            }

            if ( !SetEnvironmentVariable(varName.c_str(), varValue.c_str()) )
                nCmdResult = CMDRESULT_FAILED;
        }
        else
        {
            ScriptError( ET_REPORT, _T("- no variable name specified") );
            return CMDRESULT_INVALIDPARAM;
        }
    }

    // show the value
    tstr& varName = args.Arg(0);
        
    NppExecHelpers::StrDelLeadingTabSpaces(varName);
    NppExecHelpers::StrDelTrailingTabSpaces(varName);

    if ( varName.length() > 0 )
    {
        NppExecHelpers::StrUpper(varName);
            
        bool  bSysVarOK = false;
        DWORD nLen = GetEnvironmentVariable(varName.c_str(), NULL, 0);
        if ( nLen > 0 )
        {
            TCHAR* pStr = new TCHAR[nLen + 2];
            if ( pStr )
            {
                nLen = GetEnvironmentVariable(varName.c_str(), pStr, nLen + 1);
                if ( nLen > 0 )
                {
                    tstr S = _T("$(SYS.");
                    S += varName;
                    S += _T(") = ");
                    if ( nLen > MAX_VAR_LENGTH2SHOW )
                    {
                        S.Append( pStr, MAX_VAR_LENGTH2SHOW - 5 );
                        S += _T("(...)");
                    }
                    else
                    {
                        S += pStr;
                    }
                    m_pNppExec->GetConsole().PrintMessage( S.c_str(), isInternal );
                    bSysVarOK = true;
                }
                delete [] pStr;
            }
        }
        if ( !bSysVarOK )
        {
            tstr S = _T("$(SYS.");
            S += varName;
            S += _T(") is empty or does not exist");
            m_pNppExec->GetConsole().PrintMessage( S.c_str(), false );
        }
    }
    else
    {
        ScriptError( ET_REPORT, _T("- no variable name specified") );
        nCmdResult = CMDRESULT_INVALIDPARAM;
    }

    return nCmdResult;
}

CScriptEngine::eCmdResult CScriptEngine::DoEnvUnset(const tstr& params)
{
    if ( !reportCmdAndParams( DoEnvUnsetCommand::Name(), params, fMessageToConsole | fReportEmptyParam | fFailIfEmptyParam ) )
        return CMDRESULT_INVALIDPARAM;

    // removes the value of environment variable, restores the initial value

    eCmdResult nCmdResult = CMDRESULT_SUCCEEDED;
    tstr varName = params;
    
    NppExecHelpers::StrDelLeadingTabSpaces(varName);
    NppExecHelpers::StrDelTrailingTabSpaces(varName);

    if ( varName.length() > 0 )
    {
        NppExecHelpers::StrUpper(varName);

        BOOL bResult = FALSE;
        BOOL bRemoved = FALSE;

        {
            CCriticalSectionLockGuard lock(g_csEnvVars);
            tEnvVars::iterator itrLVar = g_LocalEnvVarNames.find(varName);
            if ( itrLVar != g_LocalEnvVarNames.end() )
            {
                // is Local Env Var; remove
                bResult = SetEnvironmentVariable( varName.c_str(), NULL );
                g_LocalEnvVarNames.erase(itrLVar);
                bRemoved = TRUE;
            }
            else
            {
                // not Local Env Var; maybe Global?
                tEnvVars::iterator itrGVar = g_GlobalEnvVars.find(varName);
                if ( itrGVar != g_GlobalEnvVars.end() )
                {
                    // is modified Global Env Var; restore the initial value
                    bResult = SetEnvironmentVariable( varName.c_str(), itrGVar->second.c_str() );
                    g_GlobalEnvVars.erase(itrGVar);
                }
                else
                {
                    // unsafe: maybe non-modified Global Env Var
                }
            }
        }

        tstr S = _T("$(SYS.");
        S += varName;
        if ( bResult )
        {
            S += _T(") has been ");
            S += (bRemoved ? _T("removed") : _T("restored"));
        }
        else
        {
            S += _T(")");
            S.Insert(0, _T("- can not unset this environment variable: "));
            nCmdResult = CMDRESULT_FAILED;
        }
        m_pNppExec->GetConsole().PrintMessage( S.c_str(), bResult ? true : false );
    }
    else
    {
        ScriptError( ET_REPORT, _T("- no variable name specified") );
        nCmdResult = CMDRESULT_INVALIDPARAM;
    }

    return nCmdResult;
}

CScriptEngine::eCmdResult CScriptEngine::DoGoTo(const tstr& params)
{
    if ( !reportCmdAndParams( DoGoToCommand::Name(), params, fMessageToConsole | fReportEmptyParam | fFailIfEmptyParam ) )
        return CMDRESULT_INVALIDPARAM;

    tstr labelName = params;
    NppExecHelpers::StrDelLeadingTabSpaces(labelName);
    NppExecHelpers::StrDelTrailingTabSpaces(labelName);
    NppExecHelpers::StrUpper(labelName);

    if ( labelName.IsEmpty() )
    {
        ScriptError( ET_REPORT, _T("- label name is empty") );
        return CMDRESULT_INVALIDPARAM;
    }

    ScriptContext& currentScript = m_execState.GetCurrentScriptContext();
    tLabels& Labels = currentScript.Labels;
    tLabels::const_iterator itrLabel = Labels.find(labelName);
    if ( itrLabel == Labels.end() )
    {
        Runtime::GetLogger().Activate(false);

        tstr Cmd;
        CListItemT<tstr>* p = m_execState.pScriptLineCurrent->GetNext();
        while ( p && (p != currentScript.CmdRange.pEnd) )
        {
            Cmd = p->GetItem();
            if ( getCmdType(m_pNppExec, Cmd) == CMDTYPE_LABEL )
            {
                NppExecHelpers::StrDelLeadingTabSpaces(Cmd);
                NppExecHelpers::StrDelTrailingTabSpaces(Cmd);
                NppExecHelpers::StrUpper(Cmd);
                if ( Labels.find(Cmd) == Labels.end() )
                {
                    Labels[Cmd] = p->GetNext(); // label points to the next command
                }
                if ( Cmd == labelName )
                {
                    itrLabel = Labels.find(labelName);
                    break;
                }
            }
            p = p->GetNext();
        }

        Runtime::GetLogger().Activate(true);

        if ( itrLabel == Labels.end() )
        {
            tstr err = _T("- no label found: ");
            err += labelName;
            ScriptError( ET_REPORT, err.c_str() );
            return CMDRESULT_FAILED;
        }
    }

    eCmdResult nCmdResult = CMDRESULT_SUCCEEDED;
    bool bContinue = true;

    m_execState.nGoToCounter++;
    if (m_execState.nGoToCounter > m_execState.nGoToMaxCount)
    {
        TCHAR szMsg[240];

        m_execState.nGoToCounter = 0;
        bContinue = false;
        ::wsprintf(szMsg, 
            _T("%s was performed more than %d times.\n") \
            _T("Abort execution of this script?\n") \
            _T("(Press Yes to abort or No to continue execution)"),
            DoGoToCommand::Name(),
            m_execState.nGoToMaxCount
        );
        if (::MessageBox(m_pNppExec->m_nppData._nppHandle, szMsg, 
                _T("NppExec Warning: Possible infinite loop"),
                  MB_YESNO | MB_ICONWARNING) == IDNO)
        {
            bContinue = true;
        }
    }

    if ( bContinue )
    {
        if ( m_execState.pScriptLineNext == NULL )
        {

            Runtime::GetLogger().Add( _T("; Ignoring GOTO as the script is being stopped") );

        }
        else
        {
            m_execState.SetScriptLineNext(itrLabel->second);

            Runtime::GetLogger().AddEx( _T("; Jumping to command item p = 0x%X { \"%s\" }"),
                m_execState.pScriptLineNext, 
                m_execState.pScriptLineNext ? m_execState.pScriptLineNext->GetItem().c_str() : _T("<NULL>") );

        }
    }
    else
    {
        ScriptError( ET_ABORT, _T("; Script execution aborted by user (from DoGoTo())") );
        nCmdResult = CMDRESULT_FAILED;
    }

    return nCmdResult;
}

enum eDecNumberType {
  DNT_NOTNUMBER = 0,
  DNT_INTNUMBER,
  DNT_FLOATNUMBER
};

static eDecNumberType isPureDecNumber(const TCHAR* s)
{
    if ( s )
    {
        if ( (*s == _T('-')) || (*s == _T('+')) )
        {
            ++s;
        }

        if ( !isDecNumChar(*s) )
            return DNT_NOTNUMBER; // we need at least one numeric char

        while ( isDecNumChar( *(++s) ) ) ;
        
        if ( *s == 0 )
            return DNT_INTNUMBER; // OK, we are at the end of string

        // end of floating number's mantissa...
        if ( *s == _T('.') )
        {
            while ( isDecNumChar( *(++s) ) ) ;

            if ( *s == 0 )
                return DNT_FLOATNUMBER; // e.g. "123." or "123.45"
        }

        // check for floating number's exponent...
        if ( (*s == _T('e')) || (*s == _T('E')) )
        {
            ++s;
            if ( (*s == _T('-')) || (*s == _T('+')) )
            {
                ++s;
            }

            while ( isDecNumChar(*s) ) 
            {
                if ( *(++s) == 0 )
                    return DNT_FLOATNUMBER; // OK, we are at the end of string
            }
        }
    }
    return DNT_NOTNUMBER;
}

enum eCondType {
    COND_NONE = 0,
    COND_EQUAL,
    COND_NOTEQUAL,
    COND_LESSTHAN,
    COND_LESSEQUAL,
    COND_GREATTHAN,
    COND_GREATEQUAL,
    COND_EQUALNOCASE
};

template<typename T> class OperandComparator
{
    public:
        OperandComparator(const T& t1, const T& t2) : m_t1(t1), m_t2(t2)
        {
        }

        OperandComparator& operator=(const OperandComparator&) = delete;

        bool compare(int condType)
        {
            bool ret = false;
            switch ( condType )
            {
                case COND_EQUAL:       ret = eq(); break;
                case COND_NOTEQUAL:    ret = ne(); break;
                case COND_LESSTHAN:    ret = lt(); break;
                case COND_LESSEQUAL:   ret = le(); break;
                case COND_GREATTHAN:   ret = gt(); break;
                case COND_GREATEQUAL:  ret = ge(); break;
                case COND_EQUALNOCASE: ret = eq_i(); break;
            }
            return ret;
        }

        bool eq() const { return (m_t1 == m_t2); }
        bool ne() const { return (m_t1 != m_t2); }
        bool lt() const { return (m_t1 < m_t2); }
        bool le() const { return (m_t1 <= m_t2); }
        bool gt() const { return (m_t1 > m_t2); }
        bool ge() const { return (m_t1 >= m_t2); }
        bool eq_i() const { return (m_t1 == m_t2); }

    protected:
        const T& m_t1;
        const T& m_t2;
};

template<> bool OperandComparator<tstr>::eq_i() const
{
    return ( ::CompareString(
        LOCALE_USER_DEFAULT, NORM_IGNORECASE, 
        m_t1.c_str(), m_t1.length(), 
        m_t2.c_str(), m_t2.length() ) == CSTR_EQUAL );
}

class CondOperand
{
    public:
        CondOperand(const tstr& s) : m_value_str(s), m_value_dbl(0), m_value_int64(0)
        {
            m_type = isPureDecNumber(s.c_str());
            if ( m_type == DNT_INTNUMBER )
            {
                m_value_int64 = c_base::_tstr2int64(s.c_str());
                m_value_dbl = static_cast<double>(m_value_int64); // to be able to compare as double
            }
            else if ( m_type == DNT_FLOATNUMBER )
            {
                m_value_dbl = _t_str2f(s.c_str());
            }
        }

        const TCHAR* type_as_str() const
        {
            switch ( type() )
            {
                case DNT_NOTNUMBER:
                    return _T("str");
                case DNT_INTNUMBER:
                    return _T("int");
                case DNT_FLOATNUMBER:
                    return _T("dbl");
            }
            return _T("");
        }

        inline eDecNumberType type() const { return m_type; }
        inline const tstr& value_str() const { return m_value_str; }
        inline double value_dbl() const { return m_value_dbl; }
        inline __int64 value_int64() const { return m_value_int64; }

    protected:
        tstr    m_value_str;
        double  m_value_dbl;
        __int64 m_value_int64;
        eDecNumberType m_type;
};

static bool IsConditionTrue(const tstr& Condition, bool* pHasSyntaxError)
{
    if ( pHasSyntaxError )  *pHasSyntaxError = false;

    CStrSplitT<TCHAR> args;

    typedef struct sCond {
        const TCHAR* szCond;
        int          nCondType;
    } tCond;

    static const tCond arrCond[] = {
        { _T("=="), COND_EQUAL       },
        { _T("!="), COND_NOTEQUAL    },
        { _T("<>"), COND_NOTEQUAL    },
        { _T(">="), COND_GREATEQUAL  },
        { _T("<="), COND_LESSEQUAL   },
        { _T("~="), COND_EQUALNOCASE },
        { _T("="),  COND_EQUAL       },
        { _T("<"),  COND_LESSTHAN    },
        { _T(">"),  COND_GREATTHAN   }
    };

    int  condType = COND_NONE;
    tstr cond;
    tstr op1;
    tstr op2;

    if ( args.SplitToArgs(Condition) == 3 )
    {
        cond = args.GetArg(1);
        op1 = args.GetArg(0);
        op2 = args.GetArg(2);

        for ( const tCond& c : arrCond )
        {
            if ( cond == c.szCond )
            {
                condType = c.nCondType;
                break;
            }
        }

        if ( condType != COND_NONE )
        {
            // restore removed quotes, if any
            int n = Condition.Find(op1);
            if ( Condition.GetAt(n - 1) == _T('\"') )
            {
                op1.Insert(0, _T('\"'));
                op1 += _T('\"');
            }

            n = Condition.RFind(op2);
            if ( Condition.GetAt(n - 1) == _T('\"') )
            {
                op2.Insert(0, _T('\"'));
                op2 += _T('\"');
            }
        }
    }
    else
    {
        for ( const tCond& c : arrCond )
        {
            int n = Condition.Find(c.szCond);
            if ( n >= 0 )
            {
                condType = c.nCondType;
                cond = c.szCond;
                op1.Copy(Condition.c_str(), n);
                op2.Copy(Condition.c_str() + n + lstrlen(c.szCond));

                NppExecHelpers::StrDelLeadingTabSpaces(op1);
                NppExecHelpers::StrDelTrailingTabSpaces(op1);

                NppExecHelpers::StrDelLeadingTabSpaces(op2);
                NppExecHelpers::StrDelTrailingTabSpaces(op2);
                break;
            }
        }
    }

    bool ret = false;

    if ( condType != COND_NONE )
    {

        Runtime::GetLogger().Add(   _T("IsConditionTrue()") );
        Runtime::GetLogger().Add(   _T("{") );
        Runtime::GetLogger().IncIndentLevel();

        // As the original quotes around op1 and op2 are kept (if any),
        // we treat "123" as a string and 123 as a number
        CondOperand co1(op1);
        CondOperand co2(op2);

        Runtime::GetLogger().AddEx( _T("op1 (%s) :  %s"), co1.type_as_str(), op1.c_str() );
        Runtime::GetLogger().AddEx( _T("op2 (%s) :  %s"), co2.type_as_str(), op2.c_str() );
        Runtime::GetLogger().AddEx( _T("condition :  %s"), cond.c_str() );

        if ( (co1.type() == DNT_NOTNUMBER) || (co2.type() == DNT_NOTNUMBER) )
        {
            // compare as string values
            ret = OperandComparator<tstr>(co1.value_str(), co2.value_str()).compare(condType);
        }
        else if ( (co1.type() == DNT_FLOATNUMBER) || (co2.type() == DNT_FLOATNUMBER) )
        {
            // compare as floating-point values
            ret = OperandComparator<double>(co1.value_dbl(), co2.value_dbl()).compare(condType);
        }
        else
        {
            // compare as integer values
            ret = OperandComparator<__int64>(co1.value_int64(), co2.value_int64()).compare(condType);
        }

        Runtime::GetLogger().DecIndentLevel();
        Runtime::GetLogger().Add(   _T("}") );

    }
    else
    {
        if ( pHasSyntaxError )  *pHasSyntaxError = true;
        ret = false;
    }

    return ret;
}

CScriptEngine::eCmdResult CScriptEngine::DoIf(const tstr& params)
{
    return doIf(params, false);
}

CScriptEngine::eCmdResult CScriptEngine::doIf(const tstr& params, bool isElseIf)
{
    enum eIfMode {
        IF_GOTO = 0,
        IF_THEN
    };

    if ( !reportCmdAndParams( DoIfCommand::Name(), params, fMessageToConsole | fReportEmptyParam | fFailIfEmptyParam ) )
        return CMDRESULT_INVALIDPARAM;

    tstr paramsUpperCase = params;
    NppExecHelpers::StrUpper(paramsUpperCase);

    static const TCHAR* arrGoTo[] = {
        _T(" GOTO "),
        _T("\tGOTO\t"),
        _T("\tGOTO "),
        _T(" GOTO\t")
    };

    static const TCHAR* arrThen[] = {
        _T(" THEN"),
        _T("\tTHEN")
    };

    int mode = IF_GOTO;
    int n = -1;
    for ( const TCHAR* const& i : arrGoTo )
    {
        n = paramsUpperCase.RFind(i);
        if ( n >= 0 )
            break;
    }

    if ( n < 0 )
    {
        if ( paramsUpperCase.EndsWith(_T(" GOTO")) || paramsUpperCase.EndsWith(_T("\tGOTO")) )
        {
            ScriptError( ET_UNPREDICTABLE, _T("- Empty goto-label specified.") );
            return CMDRESULT_INVALIDPARAM;
        }

        mode = IF_THEN;
        for ( const TCHAR* const& i : arrThen )
        {
            n = paramsUpperCase.RFind(i);
            if ( n >= 0 )
                break;
        }

        if ( n < 0 )
            Runtime::GetLogger().Add(   _T("- no \"goto\" or \"then\" specified, assuming \"then\"") );
    }

    tstr ifCondition;
    ifCondition.Copy( params.c_str(), n );
    NppExecHelpers::StrDelLeadingTabSpaces(ifCondition);
    NppExecHelpers::StrDelTrailingTabSpaces(ifCondition);

    if ( ifCondition.IsEmpty() )
    {
        ScriptError( ET_UNPREDICTABLE, _T("- Empty if-condition specified.") );
        return CMDRESULT_INVALIDPARAM;
    }

    tstr labelName;

    if ( mode == IF_GOTO )
    {
        labelName.Copy( params.c_str() + n + 6 );
        NppExecHelpers::StrDelLeadingTabSpaces(labelName);
        NppExecHelpers::StrDelTrailingTabSpaces(labelName);
        NppExecHelpers::StrUpper(labelName);

        if ( labelName.IsEmpty() )
        {
            ScriptError( ET_UNPREDICTABLE, _T("- Empty goto-label specified.") );
            return CMDRESULT_INVALIDPARAM;
        }
    }

    bool hasSyntaxError = false;
    bool isConditionOK = IsConditionTrue(ifCondition, &hasSyntaxError);

    if ( hasSyntaxError )
    {
        ScriptError( ET_UNPREDICTABLE, _T("- Syntax error in the if-condition.") );
        return CMDRESULT_FAILED;
    }

    ScriptContext& currentScript = m_execState.GetCurrentScriptContext();
    if ( mode == IF_GOTO )
    {
        if ( isConditionOK )
        {

            Runtime::GetLogger().Add(   _T("; The condition is true.") );

            DoGoTo(labelName);
        }
        else
        {

            Runtime::GetLogger().Add(   _T("; The condition is false; proceeding to next line") );

            if ( !isElseIf )
                currentScript.PushIfState(IF_MAYBE_ELSE); // ELSE may appear after IF ... GOTO
            else
                currentScript.SetIfState(IF_MAYBE_ELSE);
        }
    }
    else // IF_THEN
    {
        if ( isConditionOK )
        {

            Runtime::GetLogger().Add(   _T("; The condition is true; executing lines under the IF") );

            if ( !isElseIf )
                currentScript.PushIfState(IF_EXECUTING);
            else
                currentScript.SetIfState(IF_EXECUTING);
        }
        else
        {

            Runtime::GetLogger().Add(   _T("; The condition is false; waiting for ELSE or ENDIF") );

            if ( !isElseIf )
                currentScript.PushIfState(IF_WANT_ELSE);
            else
                currentScript.SetIfState(IF_WANT_ELSE);
        }
    }

    return CMDRESULT_SUCCEEDED;
}

CScriptEngine::eCmdResult CScriptEngine::DoInputBox(const tstr& params)
{
    if ( !reportCmdAndParams( DoInputBoxCommand::Name(), params, fMessageToConsole | fReportEmptyParam | fFailIfEmptyParam ) )
        return CMDRESULT_INVALIDPARAM;

    runInputBox(this, params.c_str());

    return CMDRESULT_SUCCEEDED;
}

CScriptEngine::eCmdResult CScriptEngine::DoLabel(const tstr& params)
{
    if ( !reportCmdAndParams( DoLabelCommand::Name(), params, fReportEmptyParam | fFailIfEmptyParam ) )
        return CMDRESULT_INVALIDPARAM;

    tstr labelName = params;
    NppExecHelpers::StrDelLeadingTabSpaces(labelName);
    NppExecHelpers::StrDelTrailingTabSpaces(labelName);
    NppExecHelpers::StrUpper(labelName);

    ScriptContext& currentScript = m_execState.GetCurrentScriptContext();
    tLabels& Labels = currentScript.Labels;
    tLabels::const_iterator itrLabel = Labels.find(labelName);
    if ( itrLabel != Labels.end() )
    {
        if ( itrLabel->second != m_execState.pScriptLineCurrent->GetNext() )
        {
            tstr err = _T("- duplicated label found: ");
            err += labelName;
            ScriptError( ET_REPORT, err.c_str() );
            return CMDRESULT_FAILED;
        }
    }

    CListItemT<tstr>* p = m_execState.pScriptLineCurrent->GetNext();
    Labels[labelName] = p; // label points to the next command

    Runtime::GetLogger().AddEx( _T("; label %s -> command item p = 0x%X { \"%s\" }"), 
        labelName.c_str(), p, p ? p->GetItem().c_str() : _T("<NULL>") );

    return CMDRESULT_SUCCEEDED;
}

static void appendOnOff(bool bOn, tstr& S1, tstr& S2)
{
    if ( bOn )
    {
        S1 += _T("+");
        S2 += _T("on");
    }
    else
    {
        S1 += _T("-");
        S2 += _T("off");
    }
}

typedef struct sIntMapping {
    int n;
    const TCHAR* str;
} tIntMapping;

static void appendInt(int n, tstr& S1, tstr& S2, const tIntMapping* mappings)
{
    TCHAR num[16];

    wsprintf( num, _T("%u"), n );
    S1 += num;
    for ( ; mappings->str != NULL; ++mappings )
    {
        if ( mappings->n == n )
        {
            S2 += mappings->str;
            break;
        }
    }
}

static void appendEnc(unsigned int enc_opt, bool bInput, tstr& S1, tstr& S2)
{
    unsigned int enc;
    const TCHAR* encName;
    TCHAR        encNum[5];

    if ( bInput )
    {
        enc = CConsoleEncodingDlg::getInputEncoding(enc_opt);
        encName = CConsoleEncodingDlg::getInputEncodingName(enc_opt);
    }
    else
    {
        enc = CConsoleEncodingDlg::getOutputEncoding(enc_opt);
        encName = CConsoleEncodingDlg::getOutputEncodingName(enc_opt);
    }

    wsprintf( encNum, _T("%u"), enc );

    S1 += encNum;
    S2 += encName;
}

CScriptEngine::eCmdResult CScriptEngine::DoNpeCmdAlias(const tstr& params)
{
    reportCmdAndParams( DoNpeCmdAliasCommand::Name(), params, fMessageToConsole );

    eCmdResult nCmdResult = CMDRESULT_SUCCEEDED;
    tstr aliasName;
    tstr aliasValue;
    tstr S;

    CCriticalSectionLockGuard lock(m_pNppExec->GetMacroVars().GetCsCmdAliases());
    CNppExecMacroVars::tMacroVars& cmdAliases = m_pNppExec->GetMacroVars().GetCmdAliases();

    if ( params.IsEmpty() )
    {

        Runtime::GetLogger().Add(   _T("; no arguments given - showing all command aliases") );

    }
    else
    {
        CStrSplitT<TCHAR> args;
        if ( args.Split( substituteMacroVarsIfNotDelayed(this, params, true), _T("="), 2) == 2 )
        {
            // set the value
            aliasName = args.Arg(0);
            aliasValue = args.Arg(1);

            NppExecHelpers::StrDelLeadingTabSpaces(aliasName);
            NppExecHelpers::StrDelTrailingTabSpaces(aliasName);
            NppExecHelpers::StrDelLeadingTabSpaces(aliasValue);
            NppExecHelpers::StrDelTrailingTabSpaces(aliasValue);

            if ( aliasName.IsEmpty() )
            {
                // in case of 'params' is a string which starts with "="
                if ( args.Split(aliasValue, _T("="), 2) == 2 )
                {
                    aliasName = _T("=");
                    aliasName += args.Arg(0);
                    aliasValue = args.Arg(1);

                    NppExecHelpers::StrDelTrailingTabSpaces(aliasName);
                    NppExecHelpers::StrDelLeadingTabSpaces(aliasValue);
                }
            }
            
            if ( aliasName.length() > 0 )
            {
                NppExecHelpers::StrUpper(aliasName);

                CNppExecMacroVars::tMacroVars::iterator itrAlias = cmdAliases.find(aliasName);
                if ( itrAlias != cmdAliases.end() )
                {
                    if ( aliasValue.length() > 0 )
                    {
                        // update
                        itrAlias->second = aliasValue;
                        
                        S = aliasName;
                        S += _T(" -> ");
                        S += aliasValue;
                    }
                    else
                    {
                        // remove
                        cmdAliases.erase(itrAlias);

                        S = _T("- command alias has been removed: ");
                        S += aliasName;
                    }

                    m_pNppExec->GetConsole().PrintMessage( S.c_str() );
                }
                else
                {
                    if ( aliasValue.length() > 0 )
                    {
                        // add
                        cmdAliases[aliasName] = aliasValue;

                        S = aliasName;
                        S += _T(" -> ");
                        S += aliasValue;
                        m_pNppExec->GetConsole().PrintMessage( S.c_str() );
                    }
                    else
                    {
                        // tried to remove non-existent command alias
                        S = _T("- no such command alias: ");
                        S += aliasName;
                        m_pNppExec->GetConsole().PrintMessage( S.c_str(), false );
                        nCmdResult = CMDRESULT_FAILED;
                    }
                }

                HMENU hMenu = m_pNppExec->GetNppMainMenu();
                if ( hMenu )
                {
                    const bool bEnable = !cmdAliases.empty();
                    ::EnableMenuItem( hMenu, g_funcItem[N_NOCMDALIASES]._cmdID,
                        MF_BYCOMMAND | (bEnable ? MF_ENABLED : MF_GRAYED) );
                }
                
                return nCmdResult;
            }
            else
            {
                if ( aliasValue.length() > 0 )
                {
                    ScriptError( ET_REPORT, _T("- no command alias name specified") );
                    return CMDRESULT_INVALIDPARAM;
                }
                else
                {
                    aliasName = _T("=");
                }
            }
        }
        else
        {
            aliasName = args.Arg(0);
            NppExecHelpers::StrDelLeadingTabSpaces(aliasName);
            NppExecHelpers::StrDelTrailingTabSpaces(aliasName);

            NppExecHelpers::StrUpper(aliasName);
        }
        
    }

    if ( cmdAliases.empty() )
    {
        m_pNppExec->GetConsole().PrintMessage( _T("- no command aliases"), false );
    }
    else
    {
        if ( aliasName.IsEmpty() )
        {
            CNppExecMacroVars::tMacroVars::const_iterator itrAlias = cmdAliases.begin();
            for ( ; itrAlias != cmdAliases.end(); ++itrAlias )
            {
                S = itrAlias->first;
                S += _T(" -> ");
                S += itrAlias->second;
                m_pNppExec->GetConsole().PrintMessage( S.c_str(), false );
            }
        }
        else
        {
            bool bFound = false;

            CNppExecMacroVars::tMacroVars::const_iterator itrAlias = cmdAliases.find(aliasName);
            if ( itrAlias != cmdAliases.end() )
            {
                S = itrAlias->first;
                S += _T(" -> ");
                S += itrAlias->second;
                m_pNppExec->GetConsole().PrintMessage( S.c_str(), false );
                bFound = true;
            }

            if ( !bFound )
            {
                tstr t = _T("- no such command alias: ");
                t += aliasName;
                m_pNppExec->GetConsole().PrintMessage( t.c_str(), false );
                nCmdResult = CMDRESULT_FAILED;
            }
        }
    }

    return nCmdResult;
}

CScriptEngine::eCmdResult CScriptEngine::DoNpeConsole(const tstr& params)
{
    reportCmdAndParams( DoNpeConsoleCommand::Name(), params, fMessageToConsole );

    eCmdResult nCmdResult = CMDRESULT_SUCCEEDED;
    bool isSilent = false;
    
    if ( params.length() > 0 )
    {
        CStrSplitT<TCHAR> args;
        int n = args.SplitToArgs(params);
        if ( n > 0 )
        {
            bool isEncChanged = false;
            bool isFilterChanged = false;
            
            for ( int i = 0; i < n; i++ )
            {
                bool isOK = false;
                tstr arg = args.GetArg(i);
                if ( arg.length() == 2 )
                {
                    // a+/a-     append mode on/off
                    // d+/d-     follow current directory on/off
                    // h+/h-     console commands history on/off
                    // m+/m-     console internal messages on/off
                    // q+/q-     command aliases on/off
                    // v+/v-     set the $(OUTPUT) variable on/off
                    // f+/f-     console output filter on/off
                    // r+/r-     console output replace filter on/off
                    // k0..3     catch NppExec's shortcut keys on/off
                    // o0/o1/o2  console output encoding: ANSI/OEM/UTF8
                    // i0/i1/i2  console input encoding: ANSI/OEM/UTF8
                    // --        silent (don't print Console mode info)

                    switch ( arg[0] )
                    {
                        case _T('a'):
                        case _T('A'):
                            if ( arg[1] == _T('+') || arg[1] == _T('-') )
                            {
                                bool bOn = ( arg[1] == _T('+') );
                                m_pNppExec->GetOptions().SetBool(OPTB_CONSOLE_APPENDMODE, bOn);
                                isOK = true;
                            }
                            break;

                        case _T('d'):
                        case _T('D'):
                            if ( arg[1] == _T('+') || arg[1] == _T('-') )
                            {
                                bool bOn = ( arg[1] == _T('+') );
                                HMENU hMenu = m_pNppExec->GetNppMainMenu();
                                if ( hMenu )
                                {
                                    ::CheckMenuItem(hMenu, g_funcItem[N_CDCURDIR]._cmdID,
                                        MF_BYCOMMAND | (bOn ? MF_CHECKED : MF_UNCHECKED));
                                }
                                m_pNppExec->GetOptions().SetBool(OPTB_CONSOLE_CDCURDIR, bOn);
                                isOK = true;
                            }
                            break;

                        case _T('h'):
                        case _T('H'):
                            if ( arg[1] == _T('+') || arg[1] == _T('-') )
                            {
                                bool bOn = ( arg[1] == _T('+') );
                                HMENU hMenu = m_pNppExec->GetNppMainMenu();
                                if ( hMenu )
                                {
                                    ::CheckMenuItem(hMenu, g_funcItem[N_CMDHISTORY]._cmdID,
                                        MF_BYCOMMAND | (bOn ? MF_CHECKED : MF_UNCHECKED));
                                }
                                m_pNppExec->GetOptions().SetBool(OPTB_CONSOLE_CMDHISTORY, bOn);
                                isOK = true;
                            }
                            break;

                        case _T('m'):
                        case _T('M'):
                            if ( arg[1] == _T('+') || arg[1] == _T('-') )
                            {
                                bool bOn = ( arg[1] == _T('-') ); // inverse
                                HMENU hMenu = m_pNppExec->GetNppMainMenu();
                                if ( hMenu )
                                {
                                    ::CheckMenuItem(hMenu, g_funcItem[N_NOINTMSGS]._cmdID,
                                        MF_BYCOMMAND | (bOn ? MF_CHECKED : MF_UNCHECKED));
                                }
                                m_pNppExec->GetOptions().SetBool(OPTB_CONSOLE_NOINTMSGS, bOn);
                                isOK = true;
                            }
                            break;

                        case _T('o'):
                        case _T('O'):
                            {
                                int enc = (int) (arg[1] - _T('0'));
                                if ( (enc >= CConsoleEncodingDlg::ENC_ANSI) &&
                                     (enc < CConsoleEncodingDlg::ENC_TOTAL) )
                                {
                                    unsigned int enc_opt = m_pNppExec->GetOptions().GetUint(OPTU_CONSOLE_ENCODING);
                                    enc_opt &= 0xFFF0;
                                    enc_opt |= enc;
                                    m_pNppExec->GetOptions().SetUint(OPTU_CONSOLE_ENCODING, enc_opt);
                                    isEncChanged = true;
                                    isOK = true;
                                }
                            }
                            break;

                        case _T('i'):
                        case _T('I'):
                            {
                                int enc = (int) (arg[1] - _T('0'));
                                if ( (enc >= CConsoleEncodingDlg::ENC_ANSI) &&
                                     (enc < CConsoleEncodingDlg::ENC_TOTAL) )
                                {
                                    unsigned int enc_opt = m_pNppExec->GetOptions().GetUint(OPTU_CONSOLE_ENCODING);
                                    enc_opt &= 0xFF0F;
                                    enc_opt |= (enc * 0x10);
                                    m_pNppExec->GetOptions().SetUint(OPTU_CONSOLE_ENCODING, enc_opt);
                                    isEncChanged = true;
                                    isOK = true;
                                }
                            }
                            break;

                        case _T('q'):
                        case _T('Q'):
                            if ( arg[1] == _T('+') || arg[1] == _T('-') )
                            {
                                bool bOn = ( arg[1] == _T('-') ); // inverse
                                HMENU hMenu = m_pNppExec->GetNppMainMenu();
                                if ( hMenu )
                                {
                                    ::CheckMenuItem(hMenu, g_funcItem[N_NOCMDALIASES]._cmdID,
                                        MF_BYCOMMAND | (bOn ? MF_CHECKED : MF_UNCHECKED));
                                }
                                m_pNppExec->GetOptions().SetBool(OPTB_CONSOLE_NOCMDALIASES, bOn);
                                isOK = true;
                            }
                            break;

                        case _T('v'):
                        case _T('V'):
                            if ( arg[1] == _T('+') || arg[1] == _T('-') )
                            {
                                bool bOn = ( arg[1] == _T('+') );
                                m_pNppExec->GetOptions().SetBool(OPTB_CONSOLE_SETOUTPUTVAR, bOn);
                                isOK = true;
                            }
                            break;

                        case _T('f'):
                        case _T('F'):
                            if ( arg[1] == _T('+') || arg[1] == _T('-') )
                            {
                                bool bOn = ( arg[1] == _T('+') );
                                m_pNppExec->GetOptions().SetBool(OPTB_CONFLTR_ENABLE, bOn);
                                isFilterChanged = true;
                                isOK = true;
                            }
                            break;

                        case _T('r'):
                        case _T('R'):
                            if ( arg[1] == _T('+') || arg[1] == _T('-') )
                            {
                                bool bOn = ( arg[1] == _T('+') );
                                m_pNppExec->GetOptions().SetBool(OPTB_CONFLTR_R_ENABLE, bOn);
                                isFilterChanged = true;
                                isOK = true;
                            }
                            break;

                        case _T('k'):
                        case _T('K'):
                            {
                                unsigned int k = (unsigned int) (arg[1] - _T('0'));
                                if ( (k >= ConsoleDlg::CSK_OFF) &&
                                     (k <= ConsoleDlg::CSK_ALL) )
                                {
                                    m_pNppExec->GetOptions().SetUint(OPTU_CONSOLE_CATCHSHORTCUTKEYS, k);
                                    isOK = true;
                                }
                            }
                            break;

                        case _T('-'):
                            if ( arg[1] == _T('-') )
                            {
                                isSilent = true;
                                isOK = true;
                            }
                            break;
                    }
                }
                if ( !isOK )
                {
                    arg.Insert( 0, _T("- unknown parameter: ") );
                    ScriptError( ET_REPORT, arg.c_str() );
                    nCmdResult = CMDRESULT_INVALIDPARAM;
                }
            }

            if ( isEncChanged )
            {
                unsigned int enc_opt = m_pNppExec->GetOptions().GetUint(OPTU_CONSOLE_ENCODING);
                if ( enc_opt > 0xFF )
                {
                    // "input as output" flag is set
                    unsigned int encIn  = CConsoleEncodingDlg::getInputEncoding(enc_opt);
                    unsigned int encOut = CConsoleEncodingDlg::getOutputEncoding(enc_opt);
                    if ( encIn != encOut )
                    {
                        // remove the "input as output" flag
                        m_pNppExec->GetOptions().SetUint(OPTU_CONSOLE_ENCODING, enc_opt & 0xFF);
                    }
                }
                m_pNppExec->UpdateConsoleEncoding();
            }

            if ( isFilterChanged )
            {
                m_pNppExec->UpdateOutputFilterMenuItem();
            }
        }
    }

    if ( !isSilent )
    {
        tstr S1 = _T("Console mode: ");
        tstr S2 = _T("; ");
        // append
        S1 += _T("a");
        S2 += _T("append: ");
        appendOnOff( m_pNppExec->GetOptions().GetBool(OPTB_CONSOLE_APPENDMODE), S1, S2 );
        // cd_curdir
        S1 += _T(" d");
        S2 += _T(", cd_curdir: ");
        appendOnOff( m_pNppExec->GetOptions().GetBool(OPTB_CONSOLE_CDCURDIR), S1, S2 );
        // cmd_history
        S1 += _T(" h");
        S2 += _T(", cmd_history: ");
        appendOnOff( m_pNppExec->GetOptions().GetBool(OPTB_CONSOLE_CMDHISTORY), S1, S2 );
        // int_msgs
        S1 += _T(" m");
        S2 += _T_RE_EOL _T("; int_msgs: ");
        appendOnOff( !m_pNppExec->GetOptions().GetBool(OPTB_CONSOLE_NOINTMSGS), S1, S2 );
        // cmd_aliases
        S1 += _T(" q");
        S2 += _T(", cmd_aliases: ");
        appendOnOff( !m_pNppExec->GetOptions().GetBool(OPTB_CONSOLE_NOCMDALIASES), S1, S2 );
        // $(OUTPUT)
        S1 += _T(" v");
        S2 += _T(", output_var: ");
        appendOnOff( m_pNppExec->GetOptions().GetBool(OPTB_CONSOLE_SETOUTPUTVAR), S1, S2 );
        // filters
        S1 += _T(" f");
        S2 += _T_RE_EOL _T("; filter: ");
        appendOnOff( m_pNppExec->GetOptions().GetBool(OPTB_CONFLTR_ENABLE), S1, S2 );
        S1 += _T(" r");
        S2 += _T(", replace_filter: ");
        appendOnOff( m_pNppExec->GetOptions().GetBool(OPTB_CONFLTR_R_ENABLE), S1, S2 );
        // shortcut_keys
        S1 += _T(" k");
        S2 += _T_RE_EOL _T("; shortcut_keys: ");
        {
            const tIntMapping mappings[] = {
                { ConsoleDlg::CSK_OFF, _T("off") },
                { ConsoleDlg::CSK_STD, _T("std") },
                { ConsoleDlg::CSK_USR, _T("usr") },
                { ConsoleDlg::CSK_ALL, _T("std+usr") },
                { 0x00, NULL } // trailing element with .str=NULL
            };
            appendInt( m_pNppExec->GetOptions().GetUint(OPTU_CONSOLE_CATCHSHORTCUTKEYS), S1, S2, mappings );
        }
        // out_enc
        unsigned int enc_opt = m_pNppExec->GetOptions().GetUint(OPTU_CONSOLE_ENCODING);
        S1 += _T(" o");
        S2 += _T_RE_EOL _T("; out_enc: ");
        appendEnc( enc_opt, false, S1, S2 );
        // in_enc
        S1 += _T(" i");
        S2 += _T(", in_enc: ");
        appendEnc( enc_opt, true, S1, S2 );
        // finally...
        m_pNppExec->GetConsole().PrintMessage( S1.c_str(), false );
        m_pNppExec->GetConsole().PrintMessage( S2.c_str(), false );
    }

    return nCmdResult;
}

CScriptEngine::eCmdResult CScriptEngine::DoNpeDebugLog(const tstr& params)
{
    reportCmdAndParams( DoNpeDebugLogCommand::Name(), params, fMessageToConsole );

    eCmdResult nCmdResult = CMDRESULT_SUCCEEDED;
    int nParam = getOnOffParam(params);
    if (nParam == PARAM_ON)
    {
        m_pNppExec->GetOptions().SetBool(OPTB_NPE_DEBUGLOG, true);
        Runtime::GetLogger().SetOutputMode(true, CNppExec::printScriptString);
    }
    else if (nParam == PARAM_OFF)
    {
        m_pNppExec->GetOptions().SetBool(OPTB_NPE_DEBUGLOG, false);
        Runtime::GetLogger().SetOutputMode(false);
    }
    else if (nParam != PARAM_EMPTY)
    {
        tstr Err = _T("- unknown parameter: ");
        Err += params;
        ScriptError( ET_REPORT, Err.c_str() );
        nCmdResult = CMDRESULT_INVALIDPARAM;
    }
    m_pNppExec->GetConsole().PrintMessage( m_pNppExec->GetOptions().GetBool(OPTB_NPE_DEBUGLOG) ? 
        _T("Debug Log is On (1)") : _T("Debug Log is Off (0)"), false );

    return nCmdResult;
}

CScriptEngine::eCmdResult CScriptEngine::DoNpeNoEmptyVars(const tstr& params)
{
    reportCmdAndParams( DoNpeNoEmptyVarsCommand::Name(), params, fMessageToConsole );

    eCmdResult nCmdResult = CMDRESULT_SUCCEEDED;
    int nParam = getOnOffParam(params);
    if (nParam == PARAM_ON)
    {
        m_pNppExec->GetOptions().SetBool(OPTB_CONSOLE_NOEMPTYVARS, true);
    }
    else if (nParam == PARAM_OFF)
    {
        m_pNppExec->GetOptions().SetBool(OPTB_CONSOLE_NOEMPTYVARS, false);
    }
    else if (nParam != PARAM_EMPTY)
    {
        tstr Err = _T("- unknown parameter: ");
        Err += params;
        ScriptError( ET_REPORT, Err.c_str() );
        nCmdResult = CMDRESULT_INVALIDPARAM;
    }
    m_pNppExec->GetConsole().PrintMessage( m_pNppExec->GetOptions().GetBool(OPTB_CONSOLE_NOEMPTYVARS) ? 
          _T("Replace empty (uninitialized) vars with empty str:  On (1)") : 
            _T("Replace empty (uninitialized) vars with empty str:  Off (0)"), false );

    return nCmdResult;
}

CScriptEngine::eCmdResult CScriptEngine::DoNpeQueue(const tstr& params)
{
    if ( !reportCmdAndParams( DoNpeQueueCommand::Name(), params, fMessageToConsole | fReportEmptyParam | fFailIfEmptyParam ) )
        return CMDRESULT_INVALIDPARAM;

    // command to be queued
    bool bUseSeparateScript = true;
    tstr Cmd = getQueuedCommand(params, bUseSeparateScript);
    Cmd = substituteMacroVarsIfNotDelayed(this, Cmd, true);

    if ( bUseSeparateScript && (m_nRunFlags & rfExitScript) == 0 )
    {
        // queuing, similar to NppExecPluginInterface's npemExecuteQueuedScript
        CListT<tstr> CmdList(Cmd);
        CNppExecCommandExecutor::ScriptableCommand * pCommand = new CNppExecCommandExecutor::DoRunScriptCommand(tstr(), CmdList, 0, CNppExecCommandExecutor::ExpirableCommand::NonExpirable);
        m_pNppExec->GetCommandExecutor().ExecuteCommand(pCommand);
    }
    else
    {
        // no queuing, using the very same script
        m_CmdList.Add(Cmd); // will be executed as the last command of m_CmdList

        //m_execState.pScriptLineCurrent->SetItem(tstr()); <-- Don't do this!
        // If this command is inside a goto-loop, then on the next iteration
        // of the loop this command would be empty!
    }

    return CMDRESULT_SUCCEEDED;
}

CScriptEngine::eCmdResult CScriptEngine::DoNppClose(const tstr& params)
{
    TCHAR szFileName[FILEPATH_BUFSIZE];

    reportCmdAndParams( DoNppCloseCommand::Name(), params, 0 );

    eCmdResult nCmdResult = CMDRESULT_SUCCEEDED;
    bool bCurrentFile = params.IsEmpty();
    if ( bCurrentFile )
    {
        Runtime::GetLogger().Add(   _T("; no file name specified - closing current file") );

        // file name is not specified i.e. closing current file
        m_pNppExec->SendNppMsg(NPPM_GETFULLCURRENTPATH,
          (WPARAM) (FILEPATH_BUFSIZE - 1), (LPARAM) szFileName);
    }
    else
    {
        Runtime::GetLogger().Add(   _T("; retrieving full file name") );
    }

    messageConsole( DoNppCloseCommand::Name(), bCurrentFile ? szFileName : params.c_str() );

    if ( bCurrentFile || m_pNppExec->nppSwitchToDocument(params.c_str(), true) )
    {
        m_pNppExec->SendNppMsg(NPPM_MENUCOMMAND, 0, IDM_FILE_CLOSE);
    }
    else
    {
        ScriptError( ET_REPORT, _T("- no such file opened") );
        nCmdResult = CMDRESULT_FAILED;
    }

    return nCmdResult;
}

CScriptEngine::eCmdResult CScriptEngine::DoNppConsole(const tstr& params)
{
    reportCmdAndParams( DoNppConsoleCommand::Name(), params, 0 );

    eCmdResult nCmdResult = CMDRESULT_SUCCEEDED;

    switch ( getOnOffParam(params) )
    {
        case PARAM_EMPTY:
            errorCmdNoParam(DoNppConsoleCommand::Name());
            nCmdResult = CMDRESULT_INVALIDPARAM;
            break;

        case PARAM_ON:
            messageConsole( DoNppConsoleCommand::Name(), _T("On") );
            m_pNppExec->showConsoleDialog(CNppExec::showIfHidden, CNppExec::scfCmdNppConsole);
            break;

        case PARAM_OFF:
            messageConsole( DoNppConsoleCommand::Name(), _T("Off") );
            //m_pNppExec->verifyConsoleDialogExists();
            if ( m_pNppExec->isConsoleDialogVisible() )
            {
                m_isClosingConsole = true;
                m_pNppExec->showConsoleDialog(CNppExec::hideIfShown, CNppExec::scfCmdNppConsole);
                m_isClosingConsole = false;
            }
            m_pNppExec->_consoleIsVisible = true;
            updateFocus();
            break;

        case PARAM_KEEP:
            messageConsole( DoNppConsoleCommand::Name(), _T("?") );
            //m_pNppExec->verifyConsoleDialogExists();
            if ( !m_pNppExec->isConsoleDialogVisible() )
            {
                m_pNppExec->_consoleIsVisible = true;
                updateFocus();
            }
            break;

        case PARAM_ENABLE:
            messageConsole( DoNppConsoleCommand::Name(), _T("+") );
            m_pNppExec->GetConsole().SetOutputEnabled(true);
            m_pNppExec->GetConsole().LockConsoleEndPos();
            break;

        case PARAM_DISABLE:
            // messageConsole( DoNppConsoleCommand::Name(), _T("-") );  --  don't output anything
            m_pNppExec->GetConsole().LockConsoleEndPos();
            m_pNppExec->GetConsole().SetOutputEnabled(false);
            break;

        default:
            // show error
            m_pNppExec->showConsoleDialog(CNppExec::showIfHidden, CNppExec::scfCmdNppConsole);
            {
                tstr Err = _T("- unknown parameter: ");
                Err += params;
                ScriptError( ET_REPORT, Err.c_str() );
                nCmdResult = CMDRESULT_INVALIDPARAM;
            }
            break;
    }

    Runtime::GetLogger().AddEx( _T("; the Console is %s"), m_pNppExec->isConsoleDialogVisible() ? _T("On") : _T("Off") );

    return nCmdResult;
}

CScriptEngine::eCmdResult CScriptEngine::DoNppExec(const tstr& params)
{
    if ( !reportCmdAndParams( DoNppExecCommand::Name(), params, fMessageToConsole | fReportEmptyParam | fFailIfEmptyParam ) )
        return CMDRESULT_INVALIDPARAM;

    // inserting commands from a script or a file into m_pNppExec->m_CmdList
        
    eCmdResult nCmdResult = CMDRESULT_SUCCEEDED;
    CStrSplitT<TCHAR> args;
    args.SplitToArgs(params);

    Runtime::GetLogger().Add(   _T("; args") );
    Runtime::GetLogger().Add(   _T("; {") );
    const tstr logIndent = Runtime::GetLogger().GetIndentStr();
    for (int i = 0; i < args.GetArgCount(); i++)
    {
        Runtime::GetLogger().AddEx( _T("; %s[%d], \"%s\""), logIndent.c_str(), i, args.GetArg(i).c_str() );
    }
    Runtime::GetLogger().Add(   _T("; }") );

    tstr scriptName = args.GetArg(0);
    NppExecHelpers::StrUpper(scriptName);
    if (!m_execState.GetScriptContextItemPtr(scriptName))
    {        
        bool bContinue = true;
          
        m_execState.nExecCounter++;
        if (m_execState.nExecCounter > m_execState.nExecMaxCount)
        {
            TCHAR szMsg[240];

            m_execState.nExecCounter = 0;
            bContinue = false;
            ::wsprintf(szMsg, 
                _T("%s was performed more than %d times.\n") \
                _T("Abort execution of this script?\n") \
                _T("(Press Yes to abort or No to continue execution)"),
                DoNppExecCommand::Name(),
                m_execState.nExecMaxCount
            );
            if (::MessageBox(m_pNppExec->m_nppData._nppHandle, szMsg, 
                    _T("NppExec Warning: Possible infinite loop"),
                      MB_YESNO | MB_ICONWARNING) == IDNO)
            {
                bContinue = true;
            }
        }

        if (bContinue)
        {  
            CFileBufT<TCHAR> fbuf;
            CNppScript       Script;
            tstr             line;
            ScriptContext    scriptContext;

            if (m_pNppExec->m_ScriptsList.GetScript(args.GetArg(0), Script))
            {
                Runtime::GetLogger().AddEx( _T("; adding commands from the script \"%s\""), args.GetArg(0).c_str() );  
                
                scriptContext.ScriptName = scriptName;
                scriptContext.CmdRange.pBegin = m_execState.pScriptLineCurrent;
                scriptContext.CmdRange.pEnd = m_execState.pScriptLineCurrent->GetNext();
                scriptContext.IsNppExeced = true;

                int n = 0;
                CListItemT<tstr>* pline = m_execState.pScriptLineCurrent;
                CListItemT<tstr>* pscriptline = Script.GetFirst();
                while (pscriptline)
                {
                    //line = "";
                    line = pscriptline->GetItem();
                    if (line.length() > 0)
                    {
                        ++n;

                        Runtime::GetLogger().AddEx( _T("; + line %d:  %s"), n, line.c_str() );
                    
                        m_pNppExec->GetMacroVars().CheckCmdArgs(line, args);
                        pline = m_CmdList.Insert(pline, true, line);
                    }
                    pscriptline = pscriptline->GetNext();
                }

                if (n != 0)
                {
                    scriptContext.CmdRange.pBegin = scriptContext.CmdRange.pBegin->GetNext();
                    m_execState.ScriptContextList.Add(scriptContext);

                    Runtime::GetLogger().AddEx( _T("; script context added: { Name = \"%s\"; CmdRange = [0x%X, 0x%X) }"), 
                        scriptContext.ScriptName.c_str(), scriptContext.CmdRange.pBegin, scriptContext.CmdRange.pEnd ); 

                }
            }
            else 
            {
                Runtime::GetLogger().AddEx( _T("; there is no script with such name (\"%s\")"), args.GetArg(0).c_str() ); 
                Runtime::GetLogger().Add(   _T("; trying to load the script from a file") ); 
              
                tstr fileName = args.GetArg(0);

                if ( fileName.EndsWith(_T(".exe")) ||
                     fileName.EndsWith(_T(".com")) ||
                     fileName.EndsWith(_T(".bat")) ||
                     fileName.EndsWith(_T(".cmd")) ||
                     fileName.EndsWith(_T(".ps1")) ||
                     fileName.EndsWith(_T(".js"))  ||
                     fileName.EndsWith(_T(".vbs")) )
                {
                    // executable or command file
                    ScriptError( ET_REPORT, _T("- syntax error: NppExec\'s script file expected, executable file given") );
                    m_pNppExec->GetConsole().PrintError( _T("; If you want to run an executable file, don\'t use NPP_EXEC command!") );
                    m_pNppExec->GetConsole().PrintError( _T("; (type HELP NPP_EXEC in the Console window for details)") );
                    m_pNppExec->GetConsole().PrintError( _T("; To run program.exe type just") );
                    m_pNppExec->GetConsole().PrintError( _T(";     program.exe") );
                    m_pNppExec->GetConsole().PrintError( _T("; instead of") );
                    m_pNppExec->GetConsole().PrintError( _T(";     npp_exec program.exe") );
                    nCmdResult = CMDRESULT_FAILED;
                }
                else
                {
                    // not executable or command file

                    if ( fileName.length() > 1 )
                    {
                        m_pNppExec->nppConvertToFullPathName(fileName, true);
                    }

                    if ( fbuf.LoadFromFile(fileName.c_str(), true, m_pNppExec->GetOptions().GetInt(OPTI_UTF8_DETECT_LENGTH)) )
                    {
                        Runtime::GetLogger().AddEx( _T("; loading the script from a file \"%s\""), fileName.c_str() );
                  
                        scriptContext.ScriptName = scriptName;
                        scriptContext.CmdRange.pBegin = m_execState.pScriptLineCurrent;
                        scriptContext.CmdRange.pEnd = m_execState.pScriptLineCurrent->GetNext();
                        scriptContext.IsNppExeced = true;

                        int n = 0;
                        CListItemT<tstr>* pline = m_execState.pScriptLineCurrent;
                        while (fbuf.GetLine(line) >= 0)
                        {
                            if (line.length() > 0)
                            {
                                ++n;
                    
                                Runtime::GetLogger().AddEx( _T("; + line %d:  %s"), n, line.c_str() );
                      
                                m_pNppExec->GetMacroVars().CheckCmdArgs(line, args);
                                pline = m_CmdList.Insert(pline, true, line);
                            }
                        }

                        if (n != 0)
                        {
                            scriptContext.CmdRange.pBegin = scriptContext.CmdRange.pBegin->GetNext();
                            m_execState.ScriptContextList.Add(scriptContext);

                            Runtime::GetLogger().AddEx( _T("; script context added: { Name = \"%s\"; CmdRange = [0x%X, 0x%X) }"), 
                                scriptContext.ScriptName.c_str(), scriptContext.CmdRange.pBegin, scriptContext.CmdRange.pEnd ); 

                        }
                    }
                    else
                    {
                        Runtime::GetLogger().Add(   _T("; there is no file with such name ") ); 
                  
                        ScriptError( ET_REPORT, _T("- can not open specified file or it is empty") );
                        nCmdResult = CMDRESULT_FAILED;
                    }
                }
                
            }
        }
        else
        {
            ScriptError( ET_ABORT, _T("; Script execution aborted to prevent possible infinite loop (from DoNppExec())") );
            nCmdResult = CMDRESULT_FAILED;
        }

    }
    else
    {
        Runtime::GetLogger().Add(   _T("; the script with the same name is being executed") );
            
        ScriptError( ET_REPORT, _T("- can\'t exec the same script at the same time") );
        nCmdResult = CMDRESULT_FAILED;
    }

    return nCmdResult;
}

CScriptEngine::eCmdResult CScriptEngine::DoNppMenuCommand(const tstr& params)
{
    if ( !reportCmdAndParams( DoNppMenuCommandCommand::Name(), params, fMessageToConsole | fReportEmptyParam | fFailIfEmptyParam ) )
        return CMDRESULT_INVALIDPARAM;

    eCmdResult nCmdResult = CMDRESULT_SUCCEEDED;
    tstr parsedItemPath, parsedSep;
    int menuItemId = m_pNppExec->nppGetMenuItemIdByName(params, parsedItemPath, parsedSep);
    if ( menuItemId > 0 )
    {
        Runtime::GetLogger().AddEx( _T("; menu item separator: %s"), 
            parsedSep.IsEmpty() ? _T("<none>") : parsedSep.c_str() );
        Runtime::GetLogger().AddEx( _T("; menu item found: id = %d"), menuItemId );

        m_pNppExec->SendNppMsg(NPPM_MENUCOMMAND, 0, menuItemId);
    }
    else
    {
        tstr err = _T("- the menu item was not found. Parsed item path: ");
        if ( !parsedItemPath.IsEmpty() )
            err += parsedItemPath;
        else
            err += _T("<none>");

        Runtime::GetLogger().AddEx( _T("; menu item separator: %s"), 
            parsedSep.IsEmpty() ? _T("<none>") : parsedSep.c_str() );

        ScriptError( ET_REPORT, err.c_str() );
        nCmdResult = CMDRESULT_FAILED;
    }

    return nCmdResult;
}

CScriptEngine::eCmdResult CScriptEngine::DoNppOpen(const tstr& params)
{
    if ( !reportCmdAndParams( DoNppOpenCommand::Name(), params, fMessageToConsole | fReportEmptyParam | fFailIfEmptyParam ) )
        return CMDRESULT_INVALIDPARAM;

    // opening a file in Notepad++

    eCmdResult nCmdResult = CMDRESULT_SUCCEEDED;
    int nFilterPos = FileFilterPos( params.c_str() );
    if ( nFilterPos < 0 )
    {
        Runtime::GetLogger().Add(   _T("; direct file (path)name specified") );
        
        // direct file (path)name
        if ( !m_pNppExec->SendNppMsg(NPPM_RELOADFILE, (WPARAM) FALSE, (LPARAM) params.c_str()) )
        {
            int nFiles = (int) m_pNppExec->SendNppMsg(NPPM_GETNBOPENFILES, 0, 0);

            if ( !m_pNppExec->SendNppMsg(NPPM_DOOPEN, (WPARAM) 0, (LPARAM) params.c_str()) )
            {
                nCmdResult = CMDRESULT_FAILED;

                DWORD dwAttr = ::GetFileAttributes(params.c_str());
                if ( (dwAttr != INVALID_FILE_ATTRIBUTES) &&
                     ((dwAttr & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) )
                {
                    // NPPM_DOOPEN unexpectedly returns 0 when a folder is specified
                    // as its argument, though in fact it does open all the files
                    // from that folder.
                    int nFilesNow = (int) m_pNppExec->SendNppMsg(NPPM_GETNBOPENFILES, 0, 0);
                    if ( nFilesNow != nFiles )
                    {
                        nCmdResult = CMDRESULT_SUCCEEDED;
                    }
                }
            }
        }
    }
    else
    {
          
        Runtime::GetLogger().Add(   _T("; file (path)name mask specified") );
            
        // file (path)name mask (e.g. "C:\Documents\*.txt")
        tstr         Path;
        tstr         Filter;
        CListT<tstr> FilesList;

        GetPathAndFilter( params.c_str(), nFilterPos, Path, Filter );

        Runtime::GetLogger().AddEx( _T("; searching \"%s\" for \"%s\""), Path.c_str(), Filter.c_str() );
          
        GetFilePathNamesList( Path.c_str(), Filter.c_str(), FilesList );

        tstr S;
        CListItemT<tstr>* ptr = FilesList.GetFirst();
        while ( ptr )
        {
            S = ptr->GetItem();
            if ( !m_pNppExec->SendNppMsg(NPPM_RELOADFILE, (WPARAM) FALSE, (LPARAM) S.c_str()) )
            {
                if ( !m_pNppExec->SendNppMsg(NPPM_DOOPEN, (WPARAM) 0, (LPARAM) S.c_str()) )
                    nCmdResult = CMDRESULT_FAILED;
            }
            ptr = ptr->GetNext();
        }
    }

    return nCmdResult;
}

CScriptEngine::eCmdResult CScriptEngine::DoNppRun(const tstr& params)
{
    if ( !reportCmdAndParams( DoNppRunCommand::Name(), params, fMessageToConsole | fReportEmptyParam | fFailIfEmptyParam ) )
        return CMDRESULT_INVALIDPARAM;

    // run a command

    eCmdResult nCmdResult = CMDRESULT_SUCCEEDED;
    tstr arg1, arg2;
    if ( SearchPath(NULL, params.c_str(), NULL, 0, NULL, NULL) != 0 )
    {
        arg1 = params;
    }
    else
    {
        CStrSplitT<TCHAR> args;
        args.SplitToArgs(params, 2);
        arg1 = args.Arg(0);
        arg2 = args.Arg(1);
    }

    Runtime::GetLogger().Add(   _T("; ShellExecute() arguments") ); 
    Runtime::GetLogger().Add(   _T("; {") );
    const tstr logIndent = Runtime::GetLogger().GetIndentStr();
    Runtime::GetLogger().AddEx( _T("; %s[file]   %s"), logIndent.c_str(), arg1.c_str() );
    Runtime::GetLogger().AddEx( _T("; %s[params] %s"), logIndent.c_str(), arg2.c_str() );
    Runtime::GetLogger().Add(   _T("; }") );
    
    tstr S;
    int  nResult = (int) (INT_PTR) ShellExecute( NULL, _T("open"), arg1.c_str(), 
           (arg2.length() > 0) ? arg2.c_str() : NULL, NULL, SW_SHOWNORMAL );
    switch ( nResult )
    {
        case 0:
            S = _T("- the operating system is out of memory or resources");
            break;
        case ERROR_FILE_NOT_FOUND:
            S = _T("- the specified file was not found");
            break;
        case ERROR_PATH_NOT_FOUND:
            S = _T("- the specified path was not found");
            break;
        case ERROR_BAD_FORMAT:
            S = _T("- the .exe file is invalid (non-Microsoft Win32 .exe or error in .exe image)");
            break;
        case SE_ERR_ACCESSDENIED:
            S = _T("- the operating system denied access to the specified file");
            break;
        case SE_ERR_ASSOCINCOMPLETE:
            S = _T("- the file name association is incomplete or invalid");
            break; 
        case SE_ERR_DDEBUSY:
            S = _T("- the DDE transaction could not be completed because other DDE transactions were being processed");
            break;
        case SE_ERR_DDEFAIL:
            S = _T("- the DDE transaction failed");
            break;
        case SE_ERR_DDETIMEOUT:
            S = _T("- the DDE transaction could not be completed because the request timed out");
            break;
        case SE_ERR_DLLNOTFOUND:
            S = _T("- the specified dynamic-link library (DLL) was not found");
            break;
        case SE_ERR_NOASSOC:
            S = _T("- there is no application associated with the given file name extension");
            break;
        case SE_ERR_OOM:
            S = _T("- there was not enough memory to complete the operation");
            break;
        case SE_ERR_SHARE:
            S = _T("- a sharing violation occurred");
            break;
        default:
            S = _T("");
            break;
    }
    if ( S.length() > 0 )
    {
        ScriptError( ET_REPORT, S.c_str() );
        nCmdResult = CMDRESULT_FAILED;
    }

    return nCmdResult;
}

CScriptEngine::eCmdResult CScriptEngine::DoNppSave(const tstr& params)
{
    eCmdResult nCmdResult = CMDRESULT_SUCCEEDED;
    TCHAR szFileName[FILEPATH_BUFSIZE];

    reportCmdAndParams( DoNppSaveCommand::Name(), params, 0 );
          
    // save a file
    bool bCurrentFile = params.IsEmpty();
    if ( bCurrentFile )
    {
          
        Runtime::GetLogger().Add(   _T("; no file name specified - saving current file") );
            
        // file name is not specified i.e. saving current file
        m_pNppExec->SendNppMsg( NPPM_GETFULLCURRENTPATH,
          (WPARAM) (FILEPATH_BUFSIZE - 1), (LPARAM) szFileName );
    }
    else
    {
          
        Runtime::GetLogger().Add(   _T("; retrieving full file name") );
            
        // file name is specified
        if ( !m_pNppExec->nppSwitchToDocument(params.c_str(), true) )
        {
            ScriptError( ET_REPORT, _T("- no such file opened") );
            return CMDRESULT_FAILED;
        }
    }
        
    messageConsole( DoNppSaveCommand::Name(), bCurrentFile ? szFileName : params.c_str() );
    
    Runtime::GetLogger().Add(   _T("; saving") );

    // TODO: use Notepad++'s message to check the document's state
    HWND hSciWnd = m_pNppExec->GetScintillaHandle();
    BOOL bModified = (BOOL) ::SendMessage(hSciWnd, SCI_GETMODIFY, 0, 0);

    if ( !m_pNppExec->SendNppMsg(NPPM_SAVECURRENTFILE, 0, 0) )
    {
        if ( bModified )
        {
            ScriptError( ET_REPORT, _T("- could not save the file") );
            nCmdResult = CMDRESULT_FAILED;
        }
        // else the document is unmodified - nothing to save
    }

    return nCmdResult;
}

CScriptEngine::eCmdResult CScriptEngine::DoNppSaveAs(const tstr& params)
{
    if ( !reportCmdAndParams( DoNppSaveAsCommand::Name(), params, fMessageToConsole | fReportEmptyParam | fFailIfEmptyParam ) )
        return CMDRESULT_INVALIDPARAM;

    eCmdResult nCmdResult = CMDRESULT_SUCCEEDED;

    Runtime::GetLogger().Add(   _T("; saving") );

    if ( !m_pNppExec->SendNppMsg(NPPM_SAVECURRENTFILEAS, 0, (LPARAM) params.c_str()) )
    {
        ScriptError( ET_REPORT, _T("- failed to save the file") );
        nCmdResult = CMDRESULT_FAILED;
    }

    return nCmdResult;
}

CScriptEngine::eCmdResult CScriptEngine::DoNppSaveAll(const tstr& params)
{
    reportCmdAndParams( DoNppSaveAllCommand::Name(), params, 0 );

    eCmdResult nCmdResult = CMDRESULT_SUCCEEDED;

    if ( !params.IsEmpty() )
    {
        ScriptError( ET_REPORT, _T("- unexpected parameter(s)") );
        nCmdResult = CMDRESULT_INVALIDPARAM;
    }
          
    m_pNppExec->GetConsole().PrintMessage(DoNppSaveAllCommand::Name());
    if ( !m_pNppExec->nppSaveAllFiles() )
        nCmdResult = CMDRESULT_FAILED;

    return nCmdResult;
}

CScriptEngine::eCmdResult CScriptEngine::DoNppSwitch(const tstr& params)
{
    if ( !reportCmdAndParams( DoNppSwitchCommand::Name(), params, fMessageToConsole | fReportEmptyParam | fFailIfEmptyParam ) )
        return CMDRESULT_INVALIDPARAM;

    eCmdResult nCmdResult = CMDRESULT_SUCCEEDED;

    if ( !m_pNppExec->nppSwitchToDocument(params.c_str(), true) )
    {
        ScriptError( ET_REPORT, _T("- no such file opened") );
        nCmdResult = CMDRESULT_FAILED;
    }

    return nCmdResult;
}

CScriptEngine::eCmdResult CScriptEngine::DoNppSetFocus(const tstr& params)
{
    if ( !reportCmdAndParams( DoNppSetFocusCommand::Name(), params, fMessageToConsole | fReportEmptyParam | fFailIfEmptyParam ) )
        return CMDRESULT_INVALIDPARAM;

    eCmdResult nCmdResult = CMDRESULT_FAILED;
    tstr type = params;
    NppExecHelpers::StrUpper(type);

    if ( type == _T("CON") )
    {
        HWND hConDlg = m_pNppExec->GetConsole().GetDialogWnd();
        if ( ::IsWindowVisible(hConDlg) )
        {
            // Note: since this code is executed in the scope of
            // CNppExecCommandExecutor::BackgroundExecuteThreadFunc,
            // it is not straightforward to set the focus
            HWND hCon = m_pNppExec->GetConsole().GetConsoleWnd();
            HWND hFocused = ::GetFocus(); // most likely returns NULL
            if ( hFocused != hCon && hFocused != hConDlg )
            {
                //m_pNppExec->SendNppMsg(NPPM_DMMSHOW, 0, (LPARAM) hConDlg);
                ::SetFocus(hConDlg);
                ::SendMessage(hConDlg, WM_SETFOCUS, 0, 0);
            }
            m_pNppExec->m_hFocusedWindowBeforeScriptStarted = hCon;
            nCmdResult = CMDRESULT_SUCCEEDED;
        }
    }
    else if ( type == _T("SCI") )
    {
        HWND hSci = m_pNppExec->GetScintillaHandle();
        if ( ::IsWindowVisible(hSci) )
        {
            // Note: since this code is executed in the scope of
            // CNppExecCommandExecutor::BackgroundExecuteThreadFunc,
            // it is not straightforward to set the focus
            HWND hFocused = ::GetFocus(); // most likely returns NULL
            if ( hFocused != hSci )
                ::SendMessage(hSci, SCI_SETFOCUS, 1, 0);
            m_pNppExec->m_hFocusedWindowBeforeScriptStarted = hSci;
            nCmdResult = CMDRESULT_SUCCEEDED;
        }
    }
    else
    {
        tstr Err = _T("- unknown parameter: ");
        Err += params;
        ScriptError( ET_REPORT, Err.c_str() );
        nCmdResult = CMDRESULT_INVALIDPARAM;
    }

    return nCmdResult;
}

CScriptEngine::eCmdResult CScriptEngine::DoNppSendMsg(const tstr& params)
{
    return doSendMsg(params, CMDTYPE_NPPSENDMSG);
}

CScriptEngine::eCmdResult CScriptEngine::DoNppSendMsgEx(const tstr& params)
{
    return doSendMsg(params, CMDTYPE_NPPSENDMSGEX);
}

CScriptEngine::eCmdResult CScriptEngine::DoSciSendMsg(const tstr& params)
{
    return doSendMsg(params, CMDTYPE_SCISENDMSG);
}

CScriptEngine::eCmdResult CScriptEngine::doSendMsg(const tstr& params, int cmdType)
{
    tstr cmdName;
    HWND hWnd = NULL;
    bool isMsgEx = false;
    
    switch ( cmdType )
    {
        case CMDTYPE_NPPSENDMSG:
            cmdName = DoNppSendMsgCommand::Name();
            hWnd = m_pNppExec->m_nppData._nppHandle;
            break;

        case CMDTYPE_SCISENDMSG:
            cmdName = DoSciSendMsgCommand::Name();
            hWnd = m_pNppExec->GetScintillaHandle();
            break;

        case CMDTYPE_NPPSENDMSGEX:
            cmdName = DoNppSendMsgExCommand::Name();
            isMsgEx = true;
            break;

        default:
            // unsupported command
            return CMDRESULT_FAILED;
    }

    if ( !reportCmdAndParams( cmdName.c_str(), params, fMessageToConsole | fReportEmptyParam | fFailIfEmptyParam ) )
        return CMDRESULT_INVALIDPARAM;

    // send message to Notepad++
    
    // npp(sci)_msg <uMsg>
    // npp(sci)_msg <uMsg> <wParam>
    // npp(sci)_msg <uMsg> <wParam> <lParam>
    
    tstr val;
    tstr calcErr;
    const TCHAR* s = params.c_str();

    for ( int n = 0; n < (isMsgEx ? 2 : 1); n++ )
    {
        bool isValidParam = true;
        bool hasDblQuote = false;

        s = get_param(s, val, SEP_TABSPACE, &hasDblQuote); // uMsg or hWnd
        if ( !val.IsEmpty() )
        {

            if ( isMsgEx && n == 0 )
                Runtime::GetLogger().Add(   _T("; hWnd parameter...") );
            else
                Runtime::GetLogger().Add(   _T("; uMsg parameter...") );

            const CStrSplitT<TCHAR> args;
            m_pNppExec->GetMacroVars().CheckCmdArgs(val, args);
            m_pNppExec->GetMacroVars().CheckAllMacroVars(this, val, true);

            if ( isMsgEx && n == 0 )
            {
                // hWnd
                if ( hasDblQuote || val.GetAt(0) == _T('@') )
                {
                    isValidParam = false;
                }
                else
                {

                  #ifdef _WIN64
                    hWnd = (HWND) (UINT_PTR) c_base::_tstr2uint64( val.c_str() ); // 64-bit pointer
                  #else
                    hWnd = (HWND) (UINT_PTR) c_base::_tstr2uint( val.c_str() ); // 32-bit pointer
                  #endif

                    if ( !::IsWindow(hWnd) )
                        isValidParam = false;
                }
            }
        }
        else
        {
            if ( isMsgEx && n == 0 )
                isValidParam = false;
        }

        if ( isMsgEx && !isValidParam )
        {
            // hwnd should be a valid window handle
            ScriptError( ET_REPORT, _T("- incorrect hWnd: should be a valid window handle") );
            return CMDRESULT_FAILED;
        }
    }

    if ( (!isDecNumChar(val.GetAt(0))) &&
         (val.GetAt(0) != _T('-') || !isDecNumChar(val.GetAt(1))) )
    {
        g_fp.Calculate(m_pNppExec, val, calcErr, val); // try to calculate
    }

    UINT uMsg = c_base::_tstr2uint( val.c_str() );
    if ( uMsg == 0 )
    {
        ScriptError( ET_REPORT, _T("- uMsg is 0 (no message to send)") );
        return CMDRESULT_FAILED;
    }

    if ( *s == 0 )
    {
        // npp(sci)_msg <uMsg>

        if ( isMsgEx )
            Runtime::GetLogger().AddEx( _T("; hWnd  = %d (0x%X)"), hWnd, hWnd );
        Runtime::GetLogger().AddEx( _T("; uMsg  = %d (0x%X)"), uMsg, uMsg );
        Runtime::GetLogger().AddEx( _T("; wType = %s,  wParam = \"0\""), STR_PARAMTYPE[PT_INT] );
        Runtime::GetLogger().AddEx( _T("; lType = %s,  lParam = \"0\""), STR_PARAMTYPE[PT_INT] );

        LRESULT lResult = ::SendMessage( hWnd, uMsg, 0, 0 );
        
        // RESULT
        tstr varName;
        tstr varValue;
        TCHAR szNum[50];

        varName = MACRO_MSG_RESULT;
        szNum[0] = 0;
      #ifdef _WIN64
        c_base::_tint64_to_str( static_cast<INT_PTR>(lResult), szNum );
      #else
        c_base::_tint2str( static_cast<INT_PTR>(lResult), szNum );
      #endif
        varValue = szNum;
        m_pNppExec->GetMacroVars().SetUserMacroVar(this, varName, varValue, CNppExecMacroVars::svLocalVar); // local var
        
        // WPARAM
        varName = MACRO_MSG_WPARAM;
        varValue.Clear();
        m_pNppExec->GetMacroVars().SetUserMacroVar(this, varName, varValue, CNppExecMacroVars::svLocalVar); // local var
        
        // LPARAM
        varName = MACRO_MSG_LPARAM;
        varValue.Clear();
        m_pNppExec->GetMacroVars().SetUserMacroVar(this, varName, varValue, CNppExecMacroVars::svLocalVar); // local var
        
        return CMDRESULT_SUCCEEDED;
    }

    int  paramType[2] = { PT_INT, PT_INT };
    tstr paramValue[2] = { _T("0"), _T("0") };

    // npp(sci)_msg <uMsg> <wParam>
    // npp(sci)_msg <uMsg> <wParam> <lParam>
    
    for ( int n = 0; n < 2; n++ )
    {
        bool hasDblQuote = false;
        bool hasBracket = false;
        tstr& value = paramValue[n];
        s = get_param(s, value, SEP_TABSPACE, &hasDblQuote, &hasBracket); // wType or lType
        if ( !value.IsEmpty() )
        {

            if ( n == 0 )
                Runtime::GetLogger().Add(   _T("; wParam parameter...") );
            else
                Runtime::GetLogger().Add(   _T("; lParam parameter...") );

            const CStrSplitT<TCHAR> args;
            m_pNppExec->GetMacroVars().CheckCmdArgs(value, args);
            m_pNppExec->GetMacroVars().CheckAllMacroVars(this, value, true);

            if ( hasBracket )
            {
                if ( value.GetLastChar() != _T(']') )
                {
                    //  starts with '[', but no ']' in the end
                    ScriptError( ET_REPORT, _T("- hex-string parameter should end with \']\'") );
                    return CMDRESULT_INVALIDPARAM;
                }
            }
        }

        if ( value.GetAt(0) == _T('@') )
        {
            value.Delete(0, 1);
            if ( value.IsEmpty() && !hasDblQuote )
            {
                // empty, no '"'
                ScriptError( ET_REPORT, _T("- empty pointer parameter specified") );
                return CMDRESULT_INVALIDPARAM;
            }

            if ( hasBracket )
                paramType[n] = PT_PHEXSTR;
            else if ( hasDblQuote )
                paramType[n] = PT_PSTR;
            else
                paramType[n] = PT_PINT;
        }
        else
        {
            if ( hasBracket )
                paramType[n] = PT_HEXSTR;
            else if ( hasDblQuote )
                paramType[n] = PT_STR;
            else
                paramType[n] = PT_INT;
        }

        switch ( paramType[n] )
        {
            case PT_INT:
            case PT_PINT:
            {
                if ( (!isDecNumChar(value.GetAt(0))) &&
                     (value.GetAt(0) != _T('-') || !isDecNumChar(value.GetAt(1))) )
                {
                    if ( !g_fp.Calculate(m_pNppExec, value, calcErr, value) ) // try to calculate
                    {
                        ScriptError( ET_REPORT, _T("- string parameter specified without \"\"") );
                        return CMDRESULT_INVALIDPARAM;
                    }
                }
                break;
            }
        }

        if ( *s == 0 )
            break;
    }

    if ( isMsgEx )
        Runtime::GetLogger().AddEx( _T("; hWnd  = %d (0x%X)"), hWnd, hWnd );
    Runtime::GetLogger().AddEx( _T("; uMsg  = %d (0x%X)"), uMsg, uMsg );
    Runtime::GetLogger().AddEx( _T("; wType = %s,  wParam = \"%s\""), 
      STR_PARAMTYPE[paramType[0]], paramValue[0].c_str() );
    Runtime::GetLogger().AddEx( _T("; lType = %s,  lParam = \"%s\""), 
      STR_PARAMTYPE[paramType[1]], paramValue[1].c_str() );
        
    // npp(sci)_msg <uMsg> <wParam> <lParam>

    const int PARAM_BUF_SIZE = m_pNppExec->GetOptions().GetInt(OPTI_SENDMSG_MAXBUFLEN);
    TCHAR*   pParam[2] = { NULL, NULL };
    char*    pSciParam[2] = { NULL, NULL };
    UINT_PTR uiParam[2] = { 0, 0 };
    WPARAM   wParam = 0;
    LPARAM   lParam = 0;
    TCHAR    szNum[50];

    for ( int n = 0; n < 2; n++ )
    {
        switch ( paramType[n] )
        {
            case PT_INT:
              #ifdef _WIN64
                uiParam[n] = c_base::_tstr2int64( paramValue[n].c_str() );
              #else
                uiParam[n] = c_base::_tstr2int( paramValue[n].c_str() );
              #endif
                if ( n == 0 )
                    wParam = uiParam[n];
                else
                    lParam = uiParam[n];
                break;

            case PT_PINT:
              #ifdef _WIN64
                uiParam[n] = c_base::_tstr2int64( paramValue[n].c_str() );
              #else
                uiParam[n] = c_base::_tstr2int( paramValue[n].c_str() );
              #endif
                if ( n == 0 )
                    wParam = (WPARAM) &uiParam[n];
                else
                    lParam = (LPARAM) &uiParam[n];
                break;

            case PT_STR:
            case PT_PSTR:
            case PT_HEXSTR:
            case PT_PHEXSTR:
                if ( paramType[n] == PT_HEXSTR || paramType[n] == PT_PHEXSTR )
                {
                    int nHexBufSize = PARAM_BUF_SIZE;
                    if ( cmdType != CMDTYPE_SCISENDMSG )
                        nHexBufSize *= sizeof(TCHAR);
                    c_base::byte_t* pHexBuf = new c_base::byte_t[nHexBufSize];
                    if ( pHexBuf )
                    {
                        memset( pHexBuf, 0, nHexBufSize );

                        tstr& value = paramValue[n];
                        const TCHAR* pHexStr = value.c_str() + 1; // skip leading '['
                        value.SetSize( value.length() - 1 ); // exlude trailing ']'
                        c_base::_thexstrex2buf( pHexStr, pHexBuf, nHexBufSize - sizeof(TCHAR) );

                        if ( cmdType == CMDTYPE_SCISENDMSG )
                            pSciParam[n] = (char*) pHexBuf;
                        else
                            pParam[n] = (TCHAR*) pHexBuf;
                    }
                }
                else
                {
                    if ( cmdType == CMDTYPE_SCISENDMSG )
                    {
                        pSciParam[n] = new char[PARAM_BUF_SIZE];
                        if ( pSciParam[n] )
                        {
                            memset( pSciParam[n], 0, PARAM_BUF_SIZE*sizeof(char) );
                            if ( !paramValue[n].IsEmpty() )
                            {
                                char* pp = SciTextFromLPCTSTR(paramValue[n].c_str(), hWnd);
                                if ( pp )
                                {
                                    lstrcpyA(pSciParam[n], pp);
                                    SciTextDeleteResultPtr(pp, paramValue[n].c_str());
                                }
                            }
                        }
                    }
                    else
                    {
                        pParam[n] = new TCHAR[PARAM_BUF_SIZE];
                        if ( pParam[n] )
                        {
                            memset( pParam[n], 0, PARAM_BUF_SIZE*sizeof(TCHAR) );
                            lstrcpy( pParam[n], paramValue[n].c_str() );
                        }
                    }
                }
                if ( pParam[n] )
                {
                    if ( n == 0 )
                        wParam = (WPARAM) pParam[n];
                    else
                        lParam = (LPARAM) pParam[n];
                }
                else if ( pSciParam[n] )
                {
                    if ( n == 0 )
                        wParam = (WPARAM) pSciParam[n];
                    else
                        lParam = (LPARAM) pSciParam[n];
                }
                else
                {
                    ScriptError( ET_REPORT, _T("- could not allocate memory for a parameter") );
                    return CMDRESULT_FAILED;
                }
                break;
        }
    }

    LRESULT lResult = ::SendMessage( hWnd, uMsg, wParam, lParam );

    for ( int n = 0; n < 2; n++ )
    {
        if ( paramType[n] == PT_STR || paramType[n] == PT_HEXSTR )
        {
            if ( pSciParam[n] )
            {
                #ifdef UNICODE
                    delete [] pSciParam[n];
                #else
                    if ( pSciParam[n] != paramValue[n].c_str() )
                        delete [] pSciParam[n];
                #endif

                pSciParam[n] = NULL;
            }
            if ( pParam[n] )
            {
                delete [] pParam[n];
                pParam[n] = NULL;
            }
        }
    }

    int  i;
    tstr varName;
    tstr varValue;
    
    for ( int n = 0; n < 2; n++ )
    {
        varName = (n == 0) ? MACRO_MSG_WPARAM : MACRO_MSG_LPARAM;

        switch ( paramType[n] )
        {
            case PT_PINT:
                szNum[0] = 0;
              #ifdef _WIN64
                c_base::_tint64_to_str( static_cast<INT_PTR>(uiParam[n]), szNum );
              #else
                c_base::_tint2str( static_cast<INT_PTR>(uiParam[n]), szNum );
              #endif
                varValue = szNum;
                m_pNppExec->GetMacroVars().SetUserMacroVar(this, varName, varValue, CNppExecMacroVars::svLocalVar); // local var
                break;

            case PT_PSTR:
                if ( cmdType == CMDTYPE_SCISENDMSG )
                {
                    TCHAR* pp = SciTextToLPTSTR(pSciParam[n], hWnd);
                    varValue = pp;
                    SciTextDeleteResultPtr(pp, pSciParam[n]);
                    delete [] pSciParam[n];
                    pSciParam[n] = NULL;
                }
                else
                {
                    varValue = pParam[n];
                    delete [] pParam[n];
                    pParam[n] = NULL;
                }
                for ( i = varValue.length() - 1; i >= 0; i-- )
                {
                    const TCHAR ch = varValue[i];
                    if ( ch == _T('\r') || ch == _T('\n') )
                        varValue.Delete(i, 1); // exclude trailing '\r' or '\n'
                    else
                        break;
                }
                m_pNppExec->GetMacroVars().SetUserMacroVar(this, varName, varValue, CNppExecMacroVars::svLocalVar); // local var
                break;

            case PT_PHEXSTR:
                varValue = _T("[ ");
                {
                    const int nBytesInLine = 16;
                    const int nBytesToShow = 256;
                    c_base::byte_t* pHexBuf;
                    c_base::byte_t* p;
                    TCHAR szBytesLine[3*nBytesInLine + 2];

                    if ( cmdType == CMDTYPE_SCISENDMSG )
                        pHexBuf = (c_base::byte_t *) pSciParam[n];
                    else
                        pHexBuf = (c_base::byte_t *) pParam[n];

                    p = pHexBuf;
                    while ( p < pHexBuf + nBytesToShow )
                    {
                        if ( p != pHexBuf )  varValue += _T_RE_EOL _T("  ");
                        c_base::_tbuf2hexstr(p, nBytesInLine, szBytesLine, 3*nBytesInLine + 2, _T(" "));
                        varValue += szBytesLine;
                        p += nBytesInLine;
                    }

                    delete [] pHexBuf;
                    if ( cmdType == CMDTYPE_SCISENDMSG )
                        pSciParam[n] = NULL;
                    else
                        pParam[n] = NULL;
                }
                varValue += _T(" ]");
                m_pNppExec->GetMacroVars().SetUserMacroVar(this, varName, varValue, CNppExecMacroVars::svLocalVar); // local var
                break;

            default:
                varValue.Clear();
                m_pNppExec->GetMacroVars().SetUserMacroVar(this, varName, varValue, CNppExecMacroVars::svLocalVar); // local var
                break;
        }
    }

    varName = MACRO_MSG_RESULT;
    szNum[0] = 0;
  #ifdef _WIN64
    c_base::_tint64_to_str( static_cast<INT_PTR>(lResult), szNum );
  #else
    c_base::_tint2str( static_cast<INT_PTR>(lResult), szNum );
  #endif
    varValue = szNum;
    m_pNppExec->GetMacroVars().SetUserMacroVar(this, varName, varValue, CNppExecMacroVars::svLocalVar); // local var

    return CMDRESULT_SUCCEEDED;
}

CScriptEngine::eCmdResult CScriptEngine::doSciFindReplace(const tstr& params, eCmdType cmdType)
{
    if ( !reportCmdAndParams( GetCommandRegistry().GetCmdNameByType(cmdType), params, fMessageToConsole | fReportEmptyParam | fFailIfEmptyParam ) )
        return CMDRESULT_INVALIDPARAM;

    // 1. Preparing the arguments...
    const CStrSplitT<TCHAR> cmdArgs;
    CStrSplitT<TCHAR> args;
    const int nArgs = args.SplitToArgs(params, (cmdType == CMDTYPE_SCIFIND) ? 2 : 3);
    if ( nArgs < 2 )
    {
        TCHAR szErr[64];
        wsprintf(szErr, _T("not enough parameters: %s expected, %d given"), (cmdType == CMDTYPE_SCIFIND) ? _T("2") : _T("at least 2"), nArgs );
        errorCmdNotEnoughParams( GetCommandRegistry().GetCmdNameByType(cmdType), szErr );
        return CMDRESULT_INVALIDPARAM;
    }

    for ( int i = 0; i < nArgs; i++ )
    {
        tstr& val = args.Arg(i);
        NppExecHelpers::StrUnquote(val);
        m_pNppExec->GetMacroVars().CheckCmdArgs(val, cmdArgs);
        m_pNppExec->GetMacroVars().CheckAllMacroVars(this, val, true);
    }

    // 2. Search flags...
    tstr sFlags = args.Arg(0);
    NppExecHelpers::StrUnquote(sFlags);
    CNppExecMacroVars::StrCalc(sFlags, m_pNppExec).Process();
    HWND hSci = m_pNppExec->GetScintillaHandle();
    unsigned int nSearchFlags = 0;
    unsigned int nFlags = c_base::_tstr2uint(sFlags.c_str());
    if ( nFlags & NPE_SF_MATCHCASE )
        nSearchFlags |= SCFIND_MATCHCASE;
    if ( nFlags & NPE_SF_WHOLEWORD )
        nSearchFlags |= SCFIND_WHOLEWORD;
    if ( nFlags & NPE_SF_WORDSTART )
        nSearchFlags |= SCFIND_WORDSTART;
    if ( nFlags & NPE_SF_REGEXP )
        nSearchFlags |= SCFIND_REGEXP;
    if ( nFlags & NPE_SF_POSIX )
        nSearchFlags |= SCFIND_POSIX;
    if ( nFlags & NPE_SF_CXX11REGEX )
        nSearchFlags |= SCFIND_CXX11REGEX;
    ::SendMessage(hSci, SCI_SETSEARCHFLAGS, nSearchFlags, 0);

    // 3. Search range...
    INT_PTR nSelStart = (INT_PTR) ::SendMessage(hSci, SCI_GETSELECTIONSTART, 0, 0);
    INT_PTR nSelEnd = (INT_PTR) ::SendMessage(hSci, SCI_GETSELECTIONEND, 0, 0);
    const bool bNoSelection = (nSelStart == nSelEnd);
    const bool bReplacingAll = ((nFlags & NPE_SF_REPLACEALL) && (cmdType == CMDTYPE_SCIREPLACE));
    INT_PTR nTextLength = (INT_PTR) ::SendMessage(hSci, SCI_GETTEXTLENGTH, 0, 0);
    INT_PTR nRangeStart = -1, nRangeEnd = -1;
    if ( (nFlags & NPE_SF_PRINTALL) || bReplacingAll )
    {
        if ( nFlags & NPE_SF_INWHOLETEXT )
        {
            // in the whole text
            nRangeStart = 0;
            nRangeEnd = nTextLength;
        }
        else if ( nFlags & NPE_SF_INSELECTION )
        {
            // only in the selected text
            nRangeStart = nSelStart;
            nRangeEnd = nSelEnd;
        }
        else
        {
            if ( nFlags & NPE_SF_BACKWARD )
            {
                nRangeStart = 0;
                nRangeEnd = nSelStart;
            }
            else
            {
                nRangeStart = nSelEnd;
                nRangeEnd = nTextLength;
            }
        }
    }
    else if ( nFlags & NPE_SF_INSELECTION )
    {
        nRangeStart = nSelStart;
        nRangeEnd = nSelEnd;
    }
    else
    {
        if ( nFlags & NPE_SF_BACKWARD )
        {
            nRangeStart = 0;
            nRangeEnd = nSelStart;
            // in case of NPE_SF_NEXT, no need to adjust nRangeEnd here
        }
        else
        {
            nRangeStart = nSelEnd;
            if ( ((nFlags & NPE_SF_NEXT) != 0) &&
                 bNoSelection && (nRangeStart != nTextLength) )
            {
                nRangeStart = (INT_PTR) ::SendMessage(hSci, SCI_POSITIONRELATIVE, (WPARAM) nRangeStart, (LPARAM) (1));
                // Note: just ++nRangeStart will not work for UTF-8
            }
            nRangeEnd = nTextLength;
        }
    }

    // 4. Strings to find & replace...
    int nFindStrLen = 0;
    char* pFindStr = SciTextFromLPCTSTR(args.GetArg(1).c_str(), hSci, &nFindStrLen);

    if ( nFindStrLen == 0 )
    {
        ScriptError( ET_REPORT, _T("- can not search for an empty string") );
        return CMDRESULT_INVALIDPARAM;
    }

    int nReplaceStrLen = 0;
    char* pReplaceStr = NULL;
    if ( cmdType == CMDTYPE_SCIREPLACE )
    {
        pReplaceStr = SciTextFromLPCTSTR(args.GetArg(2).c_str(), hSci, &nReplaceStrLen);
    }

    INT_PTR nFindStrLenOut = nFindStrLen;
    INT_PTR nReplaceStrLenOut = nReplaceStrLen;
    
    // 5. Searching...
    INT_PTR nPos = -1;
    if ( (nFlags & NPE_SF_PRINTALL) || bReplacingAll )
    {
        tstr S;
        unsigned int nOccurrences = 0;

        if ( bReplacingAll )
            ::SendMessage(hSci, SCI_BEGINUNDOACTION, 0, 0);

        for ( ; ; )
        {
            if ( nFlags & NPE_SF_BACKWARD )
                ::SendMessage(hSci, SCI_SETTARGETRANGE, nRangeEnd, nRangeStart);
            else
                ::SendMessage(hSci, SCI_SETTARGETRANGE, nRangeStart, nRangeEnd);

            INT_PTR pos = (INT_PTR) ::SendMessage( hSci, SCI_SEARCHINTARGET, (WPARAM) nFindStrLen, (LPARAM) (pFindStr ? pFindStr : "") );
            if ( pos < 0 )
                break;

            nPos = pos;

            pos = (INT_PTR) ::SendMessage(hSci, SCI_GETTARGETEND, 0, 0);
            nFindStrLenOut = pos - nPos;

            ++nOccurrences;

            bool isReplacing = (bReplacingAll || ((cmdType == CMDTYPE_SCIREPLACE) && (nOccurrences == 1)));
            if ( isReplacing )
            {
                if ( nFlags & (NPE_SF_REGEXP | NPE_SF_POSIX | NPE_SF_CXX11REGEX) )
                {
                    nReplaceStrLenOut = (INT_PTR) ::SendMessage( hSci, SCI_REPLACETARGETRE, (WPARAM) nReplaceStrLen, (LPARAM) (pReplaceStr ? pReplaceStr : "") );
                }
                else
                {
                    nReplaceStrLenOut = (INT_PTR) ::SendMessage( hSci, SCI_REPLACETARGET, (WPARAM) nReplaceStrLen, (LPARAM) (pReplaceStr ? pReplaceStr : "") );
                }

                if ( nFlags & NPE_SF_BACKWARD )
                {
                    nRangeEnd = nPos;
                }
                else
                {
                    nRangeStart = nPos + nReplaceStrLenOut;
                    nRangeEnd += (nReplaceStrLenOut - nFindStrLenOut);
                }

                if ( nPos + nFindStrLen <= nSelEnd )
                {
                    // each replacement affects the selection range
                    nSelEnd += (nReplaceStrLenOut - nFindStrLenOut);
                    if ( bNoSelection )
                        nSelStart = nSelEnd;
                }
            }
            else
            {
                if ( nFlags & NPE_SF_BACKWARD )
                {
                    nRangeEnd = nPos;
                }
                else
                {
                    nRangeStart = nPos + nFindStrLenOut;
                }
            }

            if ( nFlags & NPE_SF_PRINTALL )
            {
                const int MAX_STR_TO_SHOW = 128;
                const int UTF8_MAX_BYTES_PER_CHAR = 4;
                INT_PTR nLine = (INT_PTR) ::SendMessage(hSci, SCI_LINEFROMPOSITION, (WPARAM) nPos, 0);
                INT_PTR nLinePos = (INT_PTR) ::SendMessage(hSci, SCI_POSITIONFROMLINE, (WPARAM) nLine, 0);
                INT_PTR nLen = isReplacing ? nReplaceStrLenOut : nFindStrLenOut;
                int     nLenToShow = static_cast<int>(nLen);
                bool    isUtf8 = (::SendMessage(hSci, SCI_GETCODEPAGE, 0, 0) == SC_CP_UTF8);
                bool    isLenTruncated = false;
                if ( isUtf8 )
                {
                    if ( nLen > (MAX_STR_TO_SHOW*UTF8_MAX_BYTES_PER_CHAR) )
                    {
                        nLen = MAX_STR_TO_SHOW*UTF8_MAX_BYTES_PER_CHAR - 5; // number of chars, plus "(...)" will be added
                        nLenToShow = MAX_STR_TO_SHOW - 5; // number of TCHARs
                        isLenTruncated = true;
                    }
                    else
                        nLenToShow = MAX_STR_TO_SHOW; // will be adjusted later
                }
                else
                {
                    if ( nLen > MAX_STR_TO_SHOW )
                    {
                        nLen = MAX_STR_TO_SHOW - 5; // number of chars, plus "(...)" will be added
                        nLenToShow = MAX_STR_TO_SHOW - 5; // number of TCHARs
                        isLenTruncated = true;
                    }
                }
                Sci_TextRange tr;
                tr.chrg.cpMin = nPos;        // I believe Sci_CharacterRange will use INT_PTR
                tr.chrg.cpMax = nPos + nLen; // or UINT_PTR to deal with 64-bit ranges
                S.Reserve(50 + static_cast<int>(nLen)); // enough for both char* and TCHAR* buffer
                tr.lpstrText = (char *) S.c_str(); // temporary using S as a char* buffer
                ::SendMessage(hSci, SCI_GETTEXTRANGE, 0, (LPARAM) &tr);
                TCHAR* pText = SciTextToLPTSTR(tr.lpstrText, hSci); // now we have the text as TCHAR*
                INT_PTR nCharPos = ::SendMessage(hSci, SCI_COUNTCHARACTERS, nLinePos, nPos);
                // Note: nCharPos can't be just (nPos - nLinePos) because it will not work for UTF-8
                // where each character occupies up to several bytes in Scintilla's buffer
                if ( isUtf8 )
                {
                    int n = pText ? lstrlen(pText) : 0;
                    if ( n > nLenToShow )
                    {
                        if ( !isLenTruncated )
                        {
                            isLenTruncated = true;
                            nLenToShow = MAX_STR_TO_SHOW - 5;
                        }
                        pText[nLenToShow] = 0;
                    }
                }
                nLen = _t_sprintf( S.c_str(), // now using S as a TCHAR* buffer
                                 #ifdef _WIN64
                                   _T("(%I64d,%I64d)\t %s%s"), 
                                 #else
                                   _T("(%d,%d)\t %s%s"), 
                                 #endif
                                   nLine + 1, 
                                   nCharPos + 1, 
                                   pText ? pText : _T("(empty)"), 
                                   isLenTruncated ? _T("(...)") : _T("")
                                 );
                SciTextDeleteResultPtr(pText, tr.lpstrText);
                S.SetLengthValue(static_cast<int>(nLen));
                m_pNppExec->GetConsole().PrintOutput(S.c_str());
            }
        }

        if ( bReplacingAll )
            ::SendMessage(hSci, SCI_ENDUNDOACTION, 0, 0);

        S.Format(50, _T("- %u occurrences %s."), nOccurrences, bReplacingAll ? _T("replaced") : _T("found"));
        m_pNppExec->GetConsole().PrintMessage(S.c_str(), (nFlags & NPE_SF_PRINTALL) == 0);
    }
    else
    {
        if ( nFlags & NPE_SF_BACKWARD )
            ::SendMessage(hSci, SCI_SETTARGETRANGE, nRangeEnd, nRangeStart);
        else
            ::SendMessage(hSci, SCI_SETTARGETRANGE, nRangeStart, nRangeEnd);

        nPos = (INT_PTR) ::SendMessage( hSci, SCI_SEARCHINTARGET, (WPARAM) nFindStrLen, (LPARAM) (pFindStr ? pFindStr : "") );
        if ( (nPos < 0) && (nFlags & NPE_SF_INWHOLETEXT) )
        {
            // search again - in the rest of the document...
            if ( nFlags & NPE_SF_INSELECTION )
            {
                nRangeStart = 0;
                nRangeEnd = nTextLength;
            }
            else
            {
                if ( nFlags & NPE_SF_BACKWARD )
                {
                    nRangeStart = nSelStart - nFindStrLen;
                    if ( nRangeStart < 0 )
                        nRangeStart = 0;
                    nRangeEnd = nTextLength;
                }
                else
                {
                    nRangeStart = 0;
                    nRangeEnd = nSelEnd + nFindStrLen;
                    if ( nRangeEnd > nTextLength )
                        nRangeEnd = nTextLength;
                }
            }

            if ( nFlags & NPE_SF_BACKWARD )
                ::SendMessage(hSci, SCI_SETTARGETRANGE, nRangeEnd, nRangeStart);
            else
                ::SendMessage(hSci, SCI_SETTARGETRANGE, nRangeStart, nRangeEnd);
            
            nPos = (INT_PTR) ::SendMessage( hSci, SCI_SEARCHINTARGET, (WPARAM) nFindStrLen, (LPARAM) (pFindStr ? pFindStr : "") );
        }

        if ( nPos >= 0 )
        {
            INT_PTR pos = (INT_PTR) ::SendMessage(hSci, SCI_GETTARGETEND, 0, 0);
            nFindStrLenOut = pos - nPos;

            if ( cmdType == CMDTYPE_SCIREPLACE )
            {
                if ( nFlags & (NPE_SF_REGEXP | NPE_SF_POSIX | NPE_SF_CXX11REGEX) )
                {
                    nReplaceStrLenOut = (INT_PTR) ::SendMessage( hSci, SCI_REPLACETARGETRE, (WPARAM) nReplaceStrLen, (LPARAM) (pReplaceStr ? pReplaceStr : "") );
                }
                else
                {
                    nReplaceStrLenOut = (INT_PTR) ::SendMessage( hSci, SCI_REPLACETARGET, (WPARAM) nReplaceStrLen, (LPARAM) (pReplaceStr ? pReplaceStr : "") );
                }
            }
        }
    }

    SciTextDeleteResultPtr(pFindStr, args.GetArg(1).c_str());
    SciTextDeleteResultPtr(pReplaceStr, args.GetArg(2).c_str());

    // 6. Result
    {
        tstr varName;
        tstr varValue;
        TCHAR szNum[50];

        varName = MACRO_MSG_RESULT;
        szNum[0] = 0;
      #ifdef _WIN64
        c_base::_tint64_to_str(nPos, szNum);
      #else
        c_base::_tint2str(nPos, szNum);
      #endif
        varValue = szNum;
        m_pNppExec->GetMacroVars().SetUserMacroVar(this, varName, varValue, CNppExecMacroVars::svLocalVar); // local var

        if ( ((nFlags & NPE_SF_PRINTALL) == 0) && !bReplacingAll )
        {
            if ( nPos >= 0 )
            {
                tstr S = (cmdType == CMDTYPE_SCIREPLACE) ? _T("- replaced") : _T("- found");
                S += _T(" at pos ");
                S += szNum;
                m_pNppExec->GetConsole().PrintMessage( S.c_str() );
            }
            else
            {
                m_pNppExec->GetConsole().PrintMessage( _T("- not found") );
            }
        }

        varName = MACRO_MSG_WPARAM;
        szNum[0] = 0;
      #ifdef _WIN64
        c_base::_tint64_to_str(nFindStrLenOut, szNum);
      #else
        c_base::_tint2str(nFindStrLenOut, szNum);
      #endif
        varValue = szNum;
        m_pNppExec->GetMacroVars().SetUserMacroVar(this, varName, varValue, CNppExecMacroVars::svLocalVar); // local var

        varName = MACRO_MSG_LPARAM;
        if ( cmdType == CMDTYPE_SCIREPLACE )
        {
            szNum[0] = 0;
          #ifdef _WIN64
            c_base::_tint64_to_str(nReplaceStrLenOut, szNum);
          #else
            c_base::_tint2str(nReplaceStrLenOut, szNum);
          #endif
            varValue = szNum;
            m_pNppExec->GetMacroVars().SetUserMacroVar(this, varName, varValue, CNppExecMacroVars::svLocalVar); // local var
        }
        else
        {
            varValue.Clear();
            m_pNppExec->GetMacroVars().SetUserMacroVar(this, varName, varValue, CNppExecMacroVars::svLocalVar | CNppExecMacroVars::svRemoveVar); // local var
        }
    }

    // 7. Set pos/sel
    if ( nPos >= 0 )
    {
        if ( nFlags & NPE_SF_SETSEL )
        {
            INT_PTR nEndPos = (cmdType == CMDTYPE_SCIFIND) ? (nPos + nFindStrLenOut) : (nPos + nReplaceStrLenOut);
            ::SendMessage(hSci, SCI_SETSEL, (WPARAM) nPos, (LPARAM) nEndPos);
        }
        else if ( nFlags & NPE_SF_SETPOS )
        {
            ::SendMessage(hSci, SCI_GOTOPOS, (WPARAM) nPos, 0);
        }
        else if ( cmdType == CMDTYPE_SCIREPLACE )
        {
            // adjust the selection after the replacement(s)
            ::SendMessage(hSci, SCI_SETSEL, (WPARAM) nSelStart, (LPARAM) nSelEnd);
        }
    }

    return CMDRESULT_SUCCEEDED;
}

CScriptEngine::eCmdResult CScriptEngine::DoSciFind(const tstr& params)
{
    return doSciFindReplace(params, CMDTYPE_SCIFIND);
}

CScriptEngine::eCmdResult CScriptEngine::DoSciReplace(const tstr& params)
{
    return doSciFindReplace(params, CMDTYPE_SCIREPLACE);
}

CScriptEngine::eCmdResult CScriptEngine::DoProcSignal(const tstr& params)
{
    if ( !reportCmdAndParams( DoProcSignalCommand::Name(), params, fMessageToConsole | fReportEmptyParam | fFailIfEmptyParam ) )
        return CMDRESULT_INVALIDPARAM;

    if ( !m_pNppExec->GetCommandExecutor().IsChildProcessRunning() )
    {
        ScriptError( ET_REPORT, _T("- child console process is not running") );
        return CMDRESULT_FAILED;
    }

    eCmdResult nCmdResult = CMDRESULT_SUCCEEDED;
    const int nMaxKillMethods = 8;
    int nKillMethods = 0;
    CProcessKiller::eKillMethod arrKillMethods[nMaxKillMethods];
    CProcessKiller::eKillMethod nSignal;
    unsigned int nWaitTimeout = 50; // just a _small_ hard-coded timeout; meaningful value to be specified as the PROC_SIGNAL argument
    tstr arg;
    tstr unknown_args;
    CStrSplitT<TCHAR> args;
    const int n = args.SplitToArgs(params);
    for ( int i = 0; i < n && nKillMethods < nMaxKillMethods; i++ )
    {
        nSignal = CProcessKiller::killNone;
        arg = args.GetArg(i);
        NppExecHelpers::StrUpper(arg);

        if ( arg == _T("CTRLBREAK") || arg == _T("CTRL-BREAK") || arg == _T("CTRL+BREAK") )
            nSignal = CProcessKiller::killCtrlBreak;
        else if ( arg == _T("CTRLC") || arg == _T("CTRL-C") || arg == _T("CTRL+C") )
            nSignal = CProcessKiller::killCtrlC;
        else if ( arg == _T("WMCLOSE") || arg == _T("WM_CLOSE") )
            nSignal = CProcessKiller::killWmClose;
        else if ( c_base::_tis_dec_value(arg.c_str()) )
            nWaitTimeout = c_base::_tstr2uint(arg.c_str());
        else
        {
            if ( !unknown_args.IsEmpty() )
                unknown_args += _T(", ");
            unknown_args += arg;
        }

        if ( nSignal != CProcessKiller::killNone )
            arrKillMethods[nKillMethods++] = nSignal;
    }

    if ( !unknown_args.IsEmpty() )
    {
        unknown_args.Insert( 0, _T("- unknown PROC_SIGNAL parameter(s): ") );
        ScriptError( ET_REPORT, unknown_args.c_str() );
        nCmdResult = CMDRESULT_INVALIDPARAM;
    }

    if ( nKillMethods == 0 )
    {
        const TCHAR* const cszErr = _T("- no kill method recognized");
        if ( !unknown_args.IsEmpty() )
            m_pNppExec->GetConsole().PrintError( cszErr );
        else
            ScriptError( ET_REPORT, cszErr );
        return CMDRESULT_INVALIDPARAM;
    }

    std::shared_ptr<CChildProcess> pChildProc = GetRunningChildProcess();
    if ( pChildProc )
    {
        if ( pChildProc->Kill(arrKillMethods, nKillMethods, nWaitTimeout, &nSignal) )
            return CMDRESULT_FAILED;
    }

    return nCmdResult;
}

CScriptEngine::eCmdResult CScriptEngine::DoSleep(const tstr& params)
{
    if ( !reportCmdAndParams( DoSleepCommand::Name(), params, fMessageToConsole | fReportEmptyParam | fFailIfEmptyParam ) )
        return CMDRESULT_INVALIDPARAM;

    CStrSplitT<TCHAR> args;
    const int n = args.SplitToArgs(params, 2);

    if ( n == 2 )
    {
        tstr Text = args.GetArg(1);
        NppExecHelpers::StrUnquote(Text);
        if ( !Text.IsEmpty() )
        {
            m_pNppExec->GetConsole().PrintMessage( Text.c_str(), false );
        }
    }

    const TCHAR* pszMilliseconds = args.GetArg(0).c_str();
    if ( (pszMilliseconds[0] < _T('0')) || (pszMilliseconds[0] > _T('9')) )
    {
        tstr Err = _T("- positive integer expected: ");
        Err += pszMilliseconds;
        ScriptError( ET_REPORT, Err.c_str() );
        return CMDRESULT_INVALIDPARAM;
    }
    
    unsigned int nMilliseconds = c_base::_tstr2uint(pszMilliseconds);
    m_eventAbortTheScript.Wait(nMilliseconds);

    return CMDRESULT_SUCCEEDED;
}

CScriptEngine::eCmdResult CScriptEngine::doTextLoad(const tstr& params, eCmdType cmdType)
{
    bool bSelectionOnly = (cmdType == CMDTYPE_SELLOADFROM);
    if ( !reportCmdAndParams( GetCommandRegistry().GetCmdNameByType(cmdType), params, fMessageToConsole | fReportEmptyParam | fFailIfEmptyParam ) )
        return CMDRESULT_INVALIDPARAM;

    eCmdResult nCmdResult = CMDRESULT_SUCCEEDED;
    int nBytes = m_pNppExec->textLoadFrom(params.c_str(), bSelectionOnly);
    if ( nBytes < 0 )
    {
        ScriptError( ET_REPORT, _T("- can not open the file") );
        nCmdResult = CMDRESULT_FAILED;
    }

    return nCmdResult;
}

CScriptEngine::eCmdResult CScriptEngine::doTextSave(const tstr& params, eCmdType cmdType)
{
    bool bSelectionOnly = (cmdType == CMDTYPE_SELSAVETO);
    if ( !reportCmdAndParams( GetCommandRegistry().GetCmdNameByType(cmdType), params, fMessageToConsole | fReportEmptyParam | fFailIfEmptyParam ) )
        return CMDRESULT_INVALIDPARAM;

    tstr  S;
    TCHAR szCmdLine[FILEPATH_BUFSIZE + 20];
    lstrcpy(szCmdLine, params.c_str());

    eCmdResult nCmdResult = CMDRESULT_SUCCEEDED;
    int nBytes = m_pNppExec->textSaveTo(szCmdLine, bSelectionOnly);
    if ( nBytes >= 0 )
        S.Format(60, _T("- OK, %d bytes have been written to \""), nBytes);
    else
        S = _T("- failed to write to \"");
    S += (szCmdLine[0] == _T('\"')) ? (szCmdLine + 1) : szCmdLine;
    S += _T("\"");
    if ( nBytes >= 0 )
    {
        m_pNppExec->GetConsole().PrintMessage( S.c_str() );
    }
    else
    {
        if ( nBytes == -2 )
            S += _T(" (text length exceeds 2 GB)");
        ScriptError( ET_REPORT, S.c_str() );
        nCmdResult = CMDRESULT_FAILED;
    }

    return nCmdResult;
}

CScriptEngine::eCmdResult CScriptEngine::DoSelLoadFrom(const tstr& params)
{
    return doTextLoad(params, CMDTYPE_SELLOADFROM);
}

CScriptEngine::eCmdResult CScriptEngine::DoSelSaveTo(const tstr& params)
{
    return doTextSave(params, CMDTYPE_SELSAVETO);
}

CScriptEngine::eCmdResult CScriptEngine::DoSelSetText(const tstr& params)
{
    if ( !reportCmdAndParams( DoSelSetTextCommand::Name(), params, fMessageToConsole ) )
        return CMDRESULT_INVALIDPARAM;

    m_pNppExec->textSetText( params.c_str(), true );

    return CMDRESULT_SUCCEEDED;
}

CScriptEngine::eCmdResult CScriptEngine::DoSelSetTextEx(const tstr& params)
{
    if ( !reportCmdAndParams( DoSelSetTextExCommand::Name(), params, fMessageToConsole ) )
        return CMDRESULT_INVALIDPARAM;

    int  pos = params.Find( _T('\\') );
    if ( pos >= 0 )
    {
        tstr S = params;
        while ( pos < S.length() )
        {
            if ( S[pos] == _T('\\') )
            {
                switch ( S.GetAt(pos + 1) )
                {
                    case _T('n'):
                        S[pos] = _T('\n');
                        S.Delete(pos + 1, 1);
                        break;
                    case _T('r'):
                        S[pos] = _T('\r');
                        S.Delete(pos + 1, 1);
                        break;
                    case _T('t'):
                        S[pos] = _T('\t');
                        S.Delete(pos + 1, 1);
                        break;
                    case _T('\\'):
                        S.Delete(pos + 1, 1);
                        break;
                }
            }
            ++pos;
        }
        m_pNppExec->textSetText( S.c_str(), true );
    }
    else
        m_pNppExec->textSetText( params.c_str(), true );

    return CMDRESULT_SUCCEEDED;
}

CScriptEngine::eCmdResult CScriptEngine::DoTextLoadFrom(const tstr& params)
{
    return doTextLoad(params, CMDTYPE_TEXTLOADFROM);
}

CScriptEngine::eCmdResult CScriptEngine::DoTextSaveTo(const tstr& params)
{
    return doTextSave(params, CMDTYPE_TEXTSAVETO);
}

CScriptEngine::eCmdResult CScriptEngine::DoClipSetText(const tstr& params)
{
    if ( !reportCmdAndParams( DoClipSetTextCommand::Name(), params, fMessageToConsole ) )
        return CMDRESULT_INVALIDPARAM;

    HWND hWndOwner = m_pNppExec->m_nppData._nppHandle;
    return NppExecHelpers::SetClipboardText(params, hWndOwner) ? CMDRESULT_SUCCEEDED : CMDRESULT_FAILED;
}

CScriptEngine::eCmdResult CScriptEngine::DoSet(const tstr& params)
{
    reportCmdAndParams( DoSetCommand::Name(), params, fMessageToConsole );
    
    eCmdResult nCmdResult = CMDRESULT_SUCCEEDED;

    // sets the value of user's variable
    tstr CmdParams = params;
    {
        CNppExecMacroVars& MacroVars = m_pNppExec->GetMacroVars();
        const CStrSplitT<TCHAR> args;
        MacroVars.CheckCmdArgs(CmdParams, args);
        if ( !MacroVars.CheckAllMacroVars(this, CmdParams, true, CMDTYPE_SET) )
            nCmdResult = CMDRESULT_FAILED;
    }

    bool isInternal = false;
    bool bLocalVar = false;
    
    tstr varName;
    if ( CmdParams.IsEmpty() )
    {
          
        Runtime::GetLogger().Add(   _T("; no arguments given - showing all user\'s variables") );

    }
    else
    {
        varName = CmdParams;
        int k = varName.Find( _T("=") );
        if ( k >= 0 )  
        {
            varName.SetSize(k);
            isInternal = true;
        }
        NppExecHelpers::StrDelLeadingTabSpaces(varName);
        NppExecHelpers::StrDelTrailingTabSpaces(varName);
        bLocalVar = m_pNppExec->GetMacroVars().IsLocalMacroVar(varName);
        if ( varName.length() > 0 )
        {
            if ( varName.StartsWith(_T("$(")) == false )
                varName.Insert(0, _T("$("));
            if ( varName.GetLastChar() != _T(')') )
                varName.Append(_T(')'));
            NppExecHelpers::StrUpper(varName);
        }
    }

    {
        CCriticalSectionLockGuard lock(m_pNppExec->GetMacroVars().GetCsUserMacroVars());
        const CNppExecMacroVars::tMacroVars& userLocalMacroVars = m_pNppExec->GetMacroVars().GetUserLocalMacroVars(this);
        const CNppExecMacroVars::tMacroVars& userMacroVars = m_pNppExec->GetMacroVars().GetUserMacroVars();

        if ( userLocalMacroVars.empty() && bLocalVar )
        {
            m_pNppExec->GetConsole().PrintMessage( _T("- no user-defined local variables"), false );
            nCmdResult = CMDRESULT_FAILED;
        }
        else if ( userLocalMacroVars.empty() && userMacroVars.empty() )
        {
            m_pNppExec->GetConsole().PrintMessage( _T("- no user-defined variables"), false );
            nCmdResult = CMDRESULT_FAILED;
        }
        else
        {
            if ( varName.IsEmpty() )
            {
                if ( bLocalVar )
                {
                    PrintMacroVarFunc func(m_pNppExec);
                    CNppExecMacroVars::tMacroVars::const_iterator itrVar = userLocalMacroVars.begin();
                    for ( ; itrVar != userLocalMacroVars.end(); ++itrVar )
                    {
                        func(itrVar, true);
                    }
                }
                else
                    IterateUserMacroVars<PrintMacroVarFunc>(userMacroVars, userLocalMacroVars, PrintMacroVarFunc(m_pNppExec));
            }
            else
            {
                const CNppExecMacroVars::tMacroVars& macroVars = bLocalVar ? userLocalMacroVars : userMacroVars;
                CNppExecMacroVars::tMacroVars::const_iterator itrVar = macroVars.find(varName);
                if ( itrVar != macroVars.end() )
                {
                    tstr S = bLocalVar ? _T("local ") : _T("");
                    S += itrVar->first;
                    S += _T(" = ");
                    if ( itrVar->second.length() > MAX_VAR_LENGTH2SHOW )
                    {
                        S.Append( itrVar->second.c_str(), MAX_VAR_LENGTH2SHOW - 5 );
                        S += _T("(...)");
                    }
                    else
                    {
                        S += itrVar->second;
                    }
                    m_pNppExec->GetConsole().PrintMessage( S.c_str(), isInternal );
                }
                else
                {
                    tstr t = _T("- no such user\'s ");
                    if ( bLocalVar ) t += _T("local ");
                    t += _T("variable: ");
                    t += varName;
                    m_pNppExec->GetConsole().PrintMessage( t.c_str(), false );
                    nCmdResult = CMDRESULT_FAILED;
                }
            }
        }
    }

    return nCmdResult;
}

CScriptEngine::eCmdResult CScriptEngine::DoUnset(const tstr& params)
{
    reportCmdAndParams( DoUnsetCommand::Name(), params, fMessageToConsole );

    eCmdResult nCmdResult = CMDRESULT_SUCCEEDED;

    // unsets the user's variable
    tstr CmdParams = params;
    {
        CNppExecMacroVars& MacroVars = m_pNppExec->GetMacroVars();
        const CStrSplitT<TCHAR> args;
        MacroVars.CheckCmdArgs(CmdParams, args);
        if ( !MacroVars.CheckAllMacroVars(this, CmdParams, true, CMDTYPE_UNSET) )
            nCmdResult = CMDRESULT_FAILED;
    }
    
    if ( CmdParams.IsEmpty() )
    {
        errorCmdNoParam(DoUnsetCommand::Name());
        nCmdResult = CMDRESULT_INVALIDPARAM;
    }

    return nCmdResult;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

CNppExecMacroVars::CNppExecMacroVars() : m_pNppExec(0)
{
}

CNppExec* CNppExecMacroVars::GetNppExec() const
{
    return m_pNppExec;
}

void CNppExecMacroVars::SetNppExec(CNppExec* pNppExec)
{
    m_pNppExec = pNppExec;
}

bool CNppExecMacroVars::IsLocalMacroVar(tstr& varName)
{
    bool isLocal = false;
    int n = varName.Find(_T(' '));
    if ( n < 0 )
    {
        n = varName.Find(_T('\t'));
        if ( n < 0 )
            n = varName.length();
    }
    if ( n == 5 ) // length of "local"
    {
        tstr Prefix;
        Prefix.Copy( varName.c_str(), n );
        NppExecHelpers::StrLower(Prefix);
        if ( Prefix == _T("local") )
        {
            isLocal = true;
            varName.Delete(0, n);
            NppExecHelpers::StrDelLeadingTabSpaces(varName);
        }
    }
    return isLocal;
}

bool CNppExecMacroVars::ContainsMacroVar(const tstr& S)
{
    return ( (S.Find(_T("$(")) >= 0) ? true : false );
}

CNppExecMacroVars::tMacroVars& CNppExecMacroVars::GetUserLocalMacroVars(CScriptEngine* pScriptEngine)
{
    CListItemT<CScriptEngine::ScriptContext>* pScriptContextItemPtr = NULL;

    if ( pScriptEngine != nullptr )
    {
        CScriptEngine::ExecState& execState = pScriptEngine->GetExecState();
        pScriptContextItemPtr = execState.ScriptContextList.GetLast();
    }
    else
    {
        std::shared_ptr<CScriptEngine> pRunningScriptEngine = m_pNppExec->GetCommandExecutor().GetRunningScriptEngine();
        if ( pRunningScriptEngine )
        {
            CScriptEngine::ExecState& execState = pRunningScriptEngine->GetExecState();
            pScriptContextItemPtr = execState.ScriptContextList.GetLast();
        }
    }
    
    if ( pScriptContextItemPtr != NULL )
    {
        CScriptEngine::ScriptContext& scriptContext = pScriptContextItemPtr->GetItem();
        return scriptContext.LocalMacroVars;
    }

    return m_UserLocalMacroVars0;
}

CNppExecMacroVars::tMacroVars& CNppExecMacroVars::GetUserConsoleMacroVars()
{
    return m_UserConsoleMacroVars;
}

CNppExecMacroVars::tMacroVars& CNppExecMacroVars::GetUserMacroVars()
{
    return m_UserMacroVars;
}

CNppExecMacroVars::tMacroVars& CNppExecMacroVars::GetCmdAliases()
{
    return m_CmdAliases;
}

CCriticalSection& CNppExecMacroVars::GetCsUserMacroVars()
{
    return m_csUserMacroVars;
}

CCriticalSection& CNppExecMacroVars::GetCsCmdAliases()
{
    return m_csCmdAliases;
}

void CNppExecMacroVars::CheckCmdArgs(tstr& Cmd, const CStrSplitT<TCHAR>& args)
{
  
  Runtime::GetLogger().Add(   _T("CheckCmdArgs()") );
  Runtime::GetLogger().Add(   _T("{") );
  Runtime::GetLogger().IncIndentLevel();
  Runtime::GetLogger().AddEx( _T("[in]  \"%s\""), Cmd.c_str() );   
    
  if ( ContainsMacroVar(Cmd) )
  {
    tstr S = Cmd;
    NppExecHelpers::StrUpper(S);
      
    TCHAR szNum[3*sizeof(int) + 2];
    c_base::_tint2str(args.GetArgCount(), szNum);

    int   len = lstrlen(MACRO_ARGC);
    int   pos = 0;
    while ( (pos = S.Find(MACRO_ARGC, pos)) >= 0 )
    {
      S.Replace(pos, len, szNum);
      Cmd.Replace(pos, len, szNum);
      pos += lstrlen(szNum);
    }

    int  len1 = lstrlen(MACRO_ARGV);
    int  len2 = lstrlen(MACRO_RARGV);
    bool bReverse = false;
    if ( (pos = S.Find(MACRO_RARGV)) >= 0 )
    {
      len = len2;
      bReverse = true;
    }
    else if ( (pos = S.Find(MACRO_ARGV)) >= 0 )
    {
      len = len1;
      bReverse = false;
    }
    while ( pos >= 0 )
    {
      switch ( S.GetAt(pos+len) )
      {
        case 0:
        {
          const tstr& t = bReverse ? args.GetRArgs() : args.GetArgs();
          S.Replace( pos, len, t.c_str(), t.length() );
          Cmd.Replace( pos, len, t.c_str(), t.length() );
          pos += t.length();
          break;
        }
        case _T(')'):
        {
          const tstr& t = bReverse ? args.GetRArgs() : args.GetArgs();
          S.Replace( pos, len + 1, t.c_str(), t.length() );
          Cmd.Replace( pos, len + 1, t.c_str(), t.length() );
          pos += t.length();
          break;
        }
        case _T('['):
        {
          unsigned int k = 0;
          TCHAR ch = S.GetAt(pos + len + k + 1);

          szNum[0] = 0;
          while ( isDecNumChar(ch) && (k < 3*sizeof(int) + 1) )
          {
            szNum[k] = ch;
            ++k;
            ch = S.GetAt(pos + len + k + 1);
          }
          szNum[k] = 0;
          while ( (ch != _T(')')) && (ch != 0) )
          {
            ++k;
            ch = S.GetAt(pos + len + k + 1);
          }
          const tstr& t = bReverse ? args.GetRArg( _ttoi(szNum) ) : args.GetArg( _ttoi(szNum) );
          S.Replace( pos, len + k + 2, t.c_str(), t.length() );
          Cmd.Replace( pos, len + k + 2, t.c_str(), t.length() );
          pos += t.length();
          break;
        }
        default:
        {
          pos += len;
          break;
        }
      }
      
      int pos0 = pos;
      if ( (pos = S.Find(MACRO_RARGV, pos0)) >= 0 )
      {
        len = len2;
        bReverse = true;
      }
      else if ( (pos = S.Find(MACRO_ARGV, pos0)) >= 0 )
      {
        len = len1;
        bReverse = false;
      }
    }
  }

  Runtime::GetLogger().AddEx( _T("[out] \"%s\""), Cmd.c_str() );
  Runtime::GetLogger().DecIndentLevel();
  Runtime::GetLogger().Add(   _T("}") );

}

void CNppExecMacroVars::CheckCmdAliases(tstr& S, bool useLogging)
{
    if ( useLogging )
    {
        Runtime::GetLogger().Add(   _T("CheckCmdAliases()") );
        Runtime::GetLogger().Add(   _T("{") );
        Runtime::GetLogger().IncIndentLevel();
    }

    if ( !m_pNppExec->GetOptions().GetBool(OPTB_CONSOLE_NOCMDALIASES) )
    {
        if ( useLogging )
        {
            Runtime::GetLogger().Add(   _T("; command aliases enabled") );
            Runtime::GetLogger().AddEx( _T("[in]  \"%s\""), S.c_str() );
        }
        
        //NppExecHelpers::StrDelLeadingTabSpaces(S);
        
        if ( S.length() > 0 )
        {
            tstr t = S;

            if ( t.length() > 0 )
            {
                NppExecHelpers::StrUpper(t);

                CCriticalSectionLockGuard lock(GetCsCmdAliases());
                tMacroVars::const_iterator itrAlias = GetCmdAliases().begin();
                for ( ; itrAlias != GetCmdAliases().end(); ++itrAlias )
                {
                    const tstr& aliasName = itrAlias->first;
                    const int len = aliasName.length();
                    if ( (len > 0) &&
                         (c_base::_tstr_unsafe_cmpn(aliasName.c_str(), t.c_str(), len) == 0) )
                    {
                        const TCHAR ch = t.GetAt(len);
                        if ( IsTabSpaceOrEmptyChar(ch) )
                        {
                            const tstr& aliasValue = itrAlias->second;
                            S.Replace( 0, len, aliasValue.c_str(), aliasValue.length() );
                            break;
                        }
                    }
                }
            }
        }

        if ( useLogging )
        {
            Runtime::GetLogger().AddEx( _T("[out] \"%s\""), S.c_str() );
        }
    }
    else
    {
        if ( useLogging )
        {
            Runtime::GetLogger().Add(   _T("; command aliases disabled") );
        }
    }

    if ( useLogging )
    {
        Runtime::GetLogger().DecIndentLevel();
        Runtime::GetLogger().Add(   _T("}") );
    }
}

void CNppExecMacroVars::CheckNppMacroVars(tstr& S)
{
  const int    NPPVAR_COUNT = 7 + 2;
  const TCHAR* NPPVAR_STRINGS[NPPVAR_COUNT] = {
    MACRO_FILE_FULLPATH,
    MACRO_FILE_DIRPATH,
    MACRO_FILE_FULLNAME,
    MACRO_FILE_NAMEONLY,
    MACRO_FILE_EXTONLY,
    MACRO_NPP_DIRECTORY,
    MACRO_CURRENT_WORD,
    
    MACRO_CURRENT_LINE,   // (int) line number
    MACRO_CURRENT_COLUMN  // (int) column number
  };
  const UINT   NPPVAR_MESSAGES[NPPVAR_COUNT] = {
    NPPM_GETFULLCURRENTPATH,
    NPPM_GETCURRENTDIRECTORY,
    NPPM_GETFILENAME,
    NPPM_GETNAMEPART,
    NPPM_GETEXTPART,
    NPPM_GETNPPDIRECTORY,
    NPPM_GETCURRENTWORD,
    
    NPPM_GETCURRENTLINE,
    NPPM_GETCURRENTCOLUMN
  };
  
  Runtime::GetLogger().Add(   _T("CheckNppMacroVars()") );
  Runtime::GetLogger().Add(   _T("{") );
  Runtime::GetLogger().IncIndentLevel();
  Runtime::GetLogger().AddEx( _T("[in]  \"%s\""), S.c_str() );
  
  if ( ContainsMacroVar(S) )
  {
    tstr Cmd = S;
    NppExecHelpers::StrUpper(Cmd);

    const int MACRO_SIZE = CONSOLECOMMAND_BUFSIZE;
    TCHAR     szMacro[MACRO_SIZE];

    for (int j = 0; j < NPPVAR_COUNT; j++)
    {
      const TCHAR* macro_str = NPPVAR_STRINGS[j];
      const int    macro_len = lstrlen(macro_str);

      int  len = -1;
      int  pos = 0;
      while ( (pos = Cmd.Find(macro_str, /*macro_len,*/ pos)) >= 0 )
      {
        if (len < 0)
        {
          szMacro[0] = 0;
          if (j < 7)
          {
            m_pNppExec->SendNppMsg(NPPVAR_MESSAGES[j], 
              (WPARAM) (MACRO_SIZE - 1), (LPARAM) szMacro);
          }
          else
          {
            int nn = (int) m_pNppExec->SendNppMsg(NPPVAR_MESSAGES[j], 0, 0);
            wsprintf(szMacro, _T("%d"), nn);
          }
          len = lstrlen(szMacro);
        }
        S.Replace(pos, macro_len, szMacro, len);
        Cmd.Replace(pos, macro_len, szMacro, len);
        pos += len;
      }
    }
  }

  Runtime::GetLogger().AddEx( _T("[out] \"%s\""), S.c_str() );
  Runtime::GetLogger().DecIndentLevel();
  Runtime::GetLogger().Add(   _T("}") );
  
}

void CNppExecMacroVars::CheckPluginMacroVars(tstr& S)
{
  
  Runtime::GetLogger().Add(   _T("CheckPluginMacroVars()") );
  Runtime::GetLogger().Add(   _T("{") );
  Runtime::GetLogger().IncIndentLevel();
  Runtime::GetLogger().AddEx( _T("[in]  \"%s\""), S.c_str() );  
    
  if ( ContainsMacroVar(S) )
  {
    tstr Cmd = S;
    NppExecHelpers::StrUpper(Cmd);

    tstr      sub;
    int       len;
    int       pos;
    bool      bMacroOK;
    const int MACRO_SIZE = 0x200;
    TCHAR     szMacro[MACRO_SIZE];

    bMacroOK = false;
    len = lstrlen(MACRO_DOCNUMBER); // "$(#"
    pos = 0;
    while ((pos = Cmd.Find(MACRO_DOCNUMBER, pos)) >= 0)
    {
      int   k;
      tstr  snum = _T("");
      
      for (k = pos + len; k < Cmd.length(); k++)
      {
        if (isDecNumChar(Cmd[k]))
          snum += Cmd[k];
        else
          break;
      }
      for (; k < Cmd.length(); k++)
      {
        if (Cmd[k] == _T(')'))
          break;
      }
      k -= pos;
      Cmd.Delete(pos, k+1);
      S.Delete(pos, k+1);

      if (snum.length() > 0)
      {
        k = _ttoi(snum.c_str());
        if (k > 0) 
        {
          // #doc = 1..nbFiles
          if (!bMacroOK)
          {
            m_pNppExec->nppGetOpenFileNames();
            bMacroOK = true;
          }
          if (k <= m_pNppExec->npp_nbFiles)
          {
            lstrcpy(szMacro, m_pNppExec->npp_bufFileNames[k-1]);
            Cmd.Insert(pos, szMacro);
            S.Insert(pos, szMacro);
            pos += lstrlen(szMacro);
          }
        }
        else if (k == 0)
        {
          // #doc = 0 means notepad++ full path
          ::GetModuleFileName(NULL, szMacro, MACRO_SIZE-1);
          Cmd.Insert(pos, szMacro);
          S.Insert(pos, szMacro);
          pos += lstrlen(szMacro);
        }
      }

    }

    sub.Clear();
    len = lstrlen(MACRO_SYSVAR);
    pos = 0;
    while ((pos = Cmd.Find(MACRO_SYSVAR, pos)) >= 0)
    {
      bool bSysVarOK = false;
      int  i1 = pos + len;
      int  i2 = i1;
      while ((i2 < Cmd.length()) && (Cmd[i2] != _T(')')))  ++i2;
      sub.Copy(Cmd.c_str() + i1, i2 - i1);
      if (sub.length() > 0)
      {
        DWORD nLen = GetEnvironmentVariable(sub.c_str(), NULL, 0);
        if (nLen > 0)
        {
          TCHAR* pStr = new TCHAR[nLen + 2];
          if (pStr)
          {
            nLen = GetEnvironmentVariable(sub.c_str(), pStr, nLen + 1);
            if (nLen > 0)
            {
              Cmd.Replace(pos, i2 - pos + 1, pStr, nLen);
              S.Replace(pos, i2 - pos + 1, pStr, nLen);
              pos += nLen;
              bSysVarOK = true;
            }
            delete [] pStr;
          }
        }
      }
      if (!bSysVarOK)
      {
        Cmd.Replace(pos, i2 - pos + 1, _T(""), 0);
        S.Replace(pos, i2 - pos + 1, _T(""), 0);
      }
    }
    
    len = lstrlen(MACRO_LEFT_VIEW_FILE);
    pos = 0;
    while ((pos = Cmd.Find(MACRO_LEFT_VIEW_FILE, pos)) >= 0)
    {
      int ind = (int) m_pNppExec->SendNppMsg(NPPM_GETCURRENTDOCINDEX, MAIN_VIEW, MAIN_VIEW);
      if (m_pNppExec->nppGetOpenFileNamesInView(PRIMARY_VIEW, ind + 1) == ind + 1)
      {
        Cmd.Replace(pos, len, m_pNppExec->npp_bufFileNames.GetAt(ind));
        S.Replace(pos, len, m_pNppExec->npp_bufFileNames.GetAt(ind));
        pos += lstrlen(m_pNppExec->npp_bufFileNames.GetAt(ind));
      }
      else
      {
        Cmd.Replace(pos, len, _T(""), 0);
        S.Replace(pos, len, _T(""), 0);
      }
    }

    len = lstrlen(MACRO_RIGHT_VIEW_FILE);
    pos = 0;
    while ((pos = Cmd.Find(MACRO_RIGHT_VIEW_FILE, pos)) >= 0)
    {
      int ind = (int) m_pNppExec->SendNppMsg(NPPM_GETCURRENTDOCINDEX, SUB_VIEW, SUB_VIEW);
      if (m_pNppExec->nppGetOpenFileNamesInView(SECOND_VIEW, ind + 1) == ind + 1)
      {
        Cmd.Replace(pos, len, m_pNppExec->npp_bufFileNames.GetAt(ind));
        S.Replace(pos, len, m_pNppExec->npp_bufFileNames.GetAt(ind));
        pos += lstrlen(m_pNppExec->npp_bufFileNames.GetAt(ind));
      }
      else
      {
        Cmd.Replace(pos, len, _T(""), 0);
        S.Replace(pos, len, _T(""), 0);
      }
    }

    len = lstrlen(MACRO_CURRENT_WORKING_DIR);
    pos = 0;
    while ((pos = Cmd.Find(MACRO_CURRENT_WORKING_DIR, pos)) >= 0)
    {
      TCHAR szPath[FILEPATH_BUFSIZE];
      DWORD dwLen;

      szPath[0] = 0;
      dwLen = GetCurrentDirectory(FILEPATH_BUFSIZE - 1, szPath);
      Cmd.Replace(pos, len, szPath);
      S.Replace(pos, len, szPath);
      pos += (int) dwLen;
    }
    
    len = lstrlen(MACRO_PLUGINS_CONFIG_DIR);
    pos = 0;
    while ((pos = Cmd.Find(MACRO_PLUGINS_CONFIG_DIR, pos)) >= 0)
    {
      Cmd.Replace(pos, len, m_pNppExec->getConfigPath());
      S.Replace(pos, len, m_pNppExec->getConfigPath());
      pos += lstrlen(m_pNppExec->getConfigPath());
    }

    sub.Clear(); // clipboard text will be here
    bMacroOK = false;
    len = lstrlen(MACRO_CLIPBOARD_TEXT);
    pos = 0;
    while ((pos = Cmd.Find(MACRO_CLIPBOARD_TEXT, pos)) >= 0)
    {
      if (!bMacroOK)
      {
        NppExecHelpers::GetClipboardText(
          [&sub](LPCTSTR pszClipboardText) { sub = pszClipboardText; }
        );
        bMacroOK = true;
      }

      Cmd.Replace(pos, len, sub.c_str());
      S.Replace(pos, len, sub.c_str());
      pos += sub.length();
    }

    sub.Clear(); // hwnd will be here
    bMacroOK = false;
    len = lstrlen(MACRO_NPP_HWND);
    pos = 0;
    while ((pos = Cmd.Find(MACRO_NPP_HWND, pos)) >= 0)
    {
      if (!bMacroOK)
      {
        #ifdef _WIN64
          c_base::_tuint64_to_strhex((unsigned __int64)(UINT_PTR)(m_pNppExec->m_nppData._nppHandle), szMacro);
        #else
          c_base::_tuint2strhex((unsigned int)(UINT_PTR)(m_pNppExec->m_nppData._nppHandle), szMacro);
        #endif
        sub = _T("0x");
        sub += szMacro;
        bMacroOK = true;
      }

      Cmd.Replace(pos, len, sub.c_str());
      S.Replace(pos, len, sub.c_str());
      pos += sub.length();
    }

    sub.Clear(); // hwnd will be here
    bMacroOK = false;
    len = lstrlen(MACRO_SCI_HWND);
    pos = 0;
    while ((pos = Cmd.Find(MACRO_SCI_HWND, pos)) >= 0)
    {
      if (!bMacroOK)
      {
        #ifdef _WIN64
          c_base::_tuint64_to_strhex((unsigned __int64)(UINT_PTR)(m_pNppExec->GetScintillaHandle()), szMacro);
        #else
          c_base::_tuint2strhex((unsigned int)(UINT_PTR)(m_pNppExec->GetScintillaHandle()), szMacro);
        #endif
        sub = _T("0x");
        sub += szMacro;
        bMacroOK = true;
      }

      Cmd.Replace(pos, len, sub.c_str());
      S.Replace(pos, len, sub.c_str());
      pos += sub.length();
    }
  }

  Runtime::GetLogger().AddEx( _T("[out] \"%s\""), S.c_str() );
  Runtime::GetLogger().DecIndentLevel();
  Runtime::GetLogger().Add(   _T("}") );

}

bool CNppExecMacroVars::CheckUserMacroVars(CScriptEngine* pScriptEngine, tstr& S, int nCmdType )
{
  bool bResult = true;
  
  Runtime::GetLogger().Add(   _T("CheckUserMacroVars()") );
  Runtime::GetLogger().Add(   _T("{") );
  Runtime::GetLogger().IncIndentLevel();
  Runtime::GetLogger().AddEx( _T("[in]  \"%s\""), S.c_str() );
    
  if ( nCmdType == CScriptEngine::CMDTYPE_SET )
  {
    
    Runtime::GetLogger().AddEx( _T("; %s command found"), CScriptEngine::DoSetCommand::Name() );
      
    const TCHAR* DEF_OP  = _T("=");
    const TCHAR* CALC_OP = _T("~");
    const int i1 = S.Find(DEF_OP);    
    const int i2 = S.Find(CALC_OP);
    const bool bSep1 = ((i1 < i2 && i1 >= 0) || (i2 < 0));
    const TCHAR* sep = bSep1 ? DEF_OP : CALC_OP;
    CStrSplitT<TCHAR> args;

    if ( args.Split(S, sep, 2) == 2 )
    {
      tstr& varName = args.Arg(0);
      tstr& varValue = args.Arg(1);
      NppExecHelpers::StrDelLeadingTabSpaces(varName);
      NppExecHelpers::StrDelTrailingTabSpaces(varName);
      NppExecHelpers::StrDelLeadingTabSpaces(varValue);
      NppExecHelpers::StrDelTrailingTabSpaces(varValue);

      if ( ContainsMacroVar(varValue) )
      {
        CheckUserMacroVars(pScriptEngine, varValue);
        CheckEmptyMacroVars(m_pNppExec, varValue);
      
        {
          CCriticalSectionLockGuard lock(GetCsUserMacroVars());
          const tMacroVars& userLocalMacroVars = GetUserLocalMacroVars(pScriptEngine);
          const tMacroVars& userMacroVars = GetUserMacroVars();
          IterateUserMacroVars<SubstituteMacroVarFunc>(userMacroVars, userLocalMacroVars, SubstituteMacroVarFunc(varValue));
        }
      }

      if ( !bSep1 )
      {
        StrCalc(varValue, m_pNppExec).Process();
      }

      bool bLocalVar = IsLocalMacroVar(varName);

      S = bLocalVar ? _T("local ") : _T("");
      S += varName;
      S += _T(" = ");
      S += varValue;

      if ( SetUserMacroVar(pScriptEngine, varName, varValue, bLocalVar ? CNppExecMacroVars::svLocalVar : 0) )
      {
        
          Runtime::GetLogger().AddEx( _T("; OK: %s%s = %s"), bLocalVar ? _T("local ") : _T(""), varName.c_str(), varValue.c_str() );

      }
      else
      {

        Runtime::GetLogger().AddEx( _T("; failed to set %s%s = %s"), bLocalVar ? _T("local ") : _T(""), varName.c_str(), varValue.c_str() );

        bResult = false;

      }
    }
    else
    {
      // "set $(var)" returns the value of var
      // "set" returns all vars and values
    }
  }
  else if ( nCmdType == CScriptEngine::CMDTYPE_UNSET )
  {
    
    Runtime::GetLogger().AddEx( _T("; %s command found"), CScriptEngine::DoUnsetCommand::Name() );

    tstr varName = S;
    int k = varName.Find( _T("=") );
    if ( k >= 0 )  varName.SetSize(k);
    NppExecHelpers::StrDelLeadingTabSpaces(varName);
    NppExecHelpers::StrDelTrailingTabSpaces(varName);

    bool bLocalVar = IsLocalMacroVar(varName);
    unsigned int nFlags = CNppExecMacroVars::svRemoveVar;
    if ( bLocalVar )
        nFlags |= CNppExecMacroVars::svLocalVar;
    if ( SetUserMacroVar(pScriptEngine, varName, S, nFlags) )
    {

      Runtime::GetLogger().AddEx( _T("; OK: %s%s has been removed"), bLocalVar ? _T("local ") : _T(""), varName.c_str() );

      tstr t = _T("- the user\'s ");
      if ( bLocalVar ) t += _T("local ");
      t += _T("variable has been removed: ");
      t += varName;
      m_pNppExec->GetConsole().PrintMessage( t.c_str() /*, false*/ );
    }
    else
    {

      Runtime::GetLogger().AddEx( _T("; failed to unset %s%s (no such user\'s variable)"), bLocalVar ? _T("local ") : _T(""), varName.c_str() );

      tstr t = _T("- no such user\'s ");
      if ( bLocalVar ) t += _T("local ");
      t += _T("variable: ");
      t += varName;
      m_pNppExec->GetConsole().PrintMessage( t.c_str(), false );

      bResult = false;
    }
  }
  else if ( ContainsMacroVar(S) )
  {
    CCriticalSectionLockGuard lock(GetCsUserMacroVars());
    const tMacroVars& userLocalMacroVars = GetUserLocalMacroVars(pScriptEngine);
    const tMacroVars& userMacroVars = GetUserMacroVars();
    IterateUserMacroVars<SubstituteMacroVarFunc>(userMacroVars, userLocalMacroVars, SubstituteMacroVarFunc(S));
  }

  Runtime::GetLogger().AddEx( _T("[out] \"%s\""), S.c_str() );
  Runtime::GetLogger().DecIndentLevel();
  Runtime::GetLogger().Add(   _T("}") );

  return bResult;
}

void CNppExecMacroVars::CheckEmptyMacroVars(CNppExec* pNppExec, tstr& S, int nCmdType )
{
    
  Runtime::GetLogger().Add(   _T("CheckEmptyMacroVars()") );
  Runtime::GetLogger().Add(   _T("{") );
  Runtime::GetLogger().IncIndentLevel();
  Runtime::GetLogger().AddEx( _T("[in]  \"%s\""), S.c_str() );   
    
  if ( pNppExec->GetOptions().GetBool(OPTB_CONSOLE_NOEMPTYVARS) )
  {
    
    Runtime::GetLogger().Add(   _T("; the function is enabled") );  
      
    CStrSplitT<TCHAR> args;
    if ( (nCmdType == CScriptEngine::CMDTYPE_SET) || 
         (nCmdType == CScriptEngine::CMDTYPE_UNSET) )
    {
      if ( args.Split(S, _T("="), 2) == 2 )
      {
        S = args.Arg(1);
      }
    }
    
    if ( S.length() > 0 )
    {
      int i;
      while ( (i = S.Find(_T("$("))) >= 0 )
      {
        int j = S.Find(_T(')'), i + 1);
        if ( j > 0 )
          S.Delete(i, j - i + 1);
        else
          S.Delete(i, -1);
      }
    }

    if ( (nCmdType == CScriptEngine::CMDTYPE_SET) || 
         (nCmdType == CScriptEngine::CMDTYPE_UNSET) )
    {
      if (args.GetArgCount() == 2)
      {
        S.Insert( 0, _T("=") );
        S.Insert( 0, args.Arg(0) );
      }
    }
  }
  else
  {
    
    Runtime::GetLogger().Add(   _T("; the function is disabled") );  

  }

  Runtime::GetLogger().AddEx( _T("[out] \"%s\""), S.c_str() );
  Runtime::GetLogger().DecIndentLevel();
  Runtime::GetLogger().Add(   _T("}") );

}

bool CNppExecMacroVars::CheckAllMacroVars(CScriptEngine* pScriptEngine, tstr& S, bool useLogging, int nCmdType )
{
    bool bResult = false;
    
    if ( (nCmdType == CScriptEngine::CMDTYPE_SET) ||
         (nCmdType == CScriptEngine::CMDTYPE_UNSET) ||
         ContainsMacroVar(S) )
    {
        if ( !useLogging )
            Runtime::GetLogger().Activate(false);

        CheckNppMacroVars(S);
        CheckPluginMacroVars(S);
        bResult = CheckUserMacroVars(pScriptEngine, S, nCmdType); // <-- in case of CMDTYPE_SET/UNSET, sets/unsets a var
        if ( nCmdType != CScriptEngine::CMDTYPE_UNSET ) // <-- required for 'unset $(var)'
            CheckEmptyMacroVars(m_pNppExec, S, nCmdType);

        if ( !useLogging )
            Runtime::GetLogger().Activate(true);
    }

    return bResult;
}

bool CNppExecMacroVars::SetUserMacroVar(CScriptEngine* pScriptEngine, tstr& varName, const tstr& varValue, unsigned int nFlags )
{
  bool bSuccess = false;

  if ( varName.length() > 0 )
  {
    if ( varName.StartsWith(_T("$(")) == false )
      varName.Insert(0, _T("$("));
    if ( varName.GetLastChar() != _T(')') )
      varName.Append(_T(')'));
    
    NppExecHelpers::StrUpper(varName);

    {
      CCriticalSectionLockGuard lock(GetCsUserMacroVars());
      tMacroVars& macroVars = (nFlags & svLocalVar) != 0 ? GetUserLocalMacroVars(pScriptEngine) : GetUserMacroVars();
      tMacroVars::iterator itrVar = macroVars.find(varName);
      if ( itrVar != macroVars.end() )
      {
        // existing variable
        if ( (nFlags & svRemoveVar) == 0 )
          itrVar->second = varValue;
        else
          macroVars.erase(itrVar);
        bSuccess = true;
      }
      else if ( (nFlags & svRemoveVar) == 0 )
      {
        // new variable
        macroVars[varName] = varValue;
        bSuccess = true;
      }
    }
  }

  if ( bSuccess )
  {
    // handling "special" variables
    if ( (varName == MACRO_EXIT_CMD) || (varName == MACRO_EXIT_CMD_SILENT) )
    {
      // we need it for the both "set" and "unset"
      
      if ( (nFlags & svRemoveVar) == 0 )
      {
        // either @EXIT_CMD or @EXIT_CMD_SILENT can exist
        tstr pairVarName;
        if ( varName == MACRO_EXIT_CMD )
          pairVarName = MACRO_EXIT_CMD_SILENT;
        else
          pairVarName = MACRO_EXIT_CMD;
        SetUserMacroVar(pScriptEngine, pairVarName, varValue, (nFlags & svLocalVar) | svRemoveVar); // remove the other one
      }

      std::shared_ptr<CScriptEngine> pParentScriptEngine;
      bool bShareLocalVars = false;

      if ( pScriptEngine != nullptr )
      {
        pScriptEngine->SetTriedExitCmd(false);
        bShareLocalVars = pScriptEngine->IsSharingLocalVars();
        pParentScriptEngine = pScriptEngine->GetParentScriptEngine();
      }
      else
      {
        std::shared_ptr<CScriptEngine> pRunningScriptEngine = m_pNppExec->GetCommandExecutor().GetRunningScriptEngine();
        if ( pRunningScriptEngine )
        {
          pRunningScriptEngine->SetTriedExitCmd(false);
          bShareLocalVars = pRunningScriptEngine->IsSharingLocalVars();
          pParentScriptEngine = pRunningScriptEngine->GetParentScriptEngine();
        }
      }

      while ( pParentScriptEngine && bShareLocalVars )
      {
        pParentScriptEngine->SetTriedExitCmd(false);
        bShareLocalVars = pParentScriptEngine->IsSharingLocalVars();
        pParentScriptEngine = pParentScriptEngine->GetParentScriptEngine();
      }
    }
  }

  return bSuccess;
}


CNppExecMacroVars::StrCalc::StrCalc(tstr& varValue, CNppExec* pNppExec)
  : m_varValue(varValue), m_pNppExec(pNppExec), m_calcType(CT_FPARSER), m_pVar(0)
{
}

void CNppExecMacroVars::StrCalc::Process()
{
    m_calcType = StrCalc::CT_FPARSER;
    m_param.Clear();
        
    // check for 'strlen', 'strupper', 'strlower', 'substr' and so on
    m_pVar = m_varValue.c_str();
    m_pVar = get_param(m_pVar, m_param);
    if ( m_param.length() > 5 )
    {
        typedef struct sCalcType {
            const TCHAR* szCalcType;
            int nCalcType;
        } tCalcType;

        static const tCalcType arrCalcType[] = {
            { _T("STRLENSCI"),  CT_STRLENSCI  },
            { _T("STRLENS"),    CT_STRLENSCI  },
            { _T("STRLENUTF8"), CT_STRLENUTF8 },
            { _T("STRLENU"),    CT_STRLENUTF8 },
            { _T("STRLENA"),    CT_STRLEN     },
            { _T("STRLEN"),     CT_STRLEN     },
            { _T("STRUPPER"),   CT_STRUPPER   },
            { _T("STRLOWER"),   CT_STRLOWER   },
            { _T("SUBSTR"),     CT_SUBSTR     },
            { _T("STRFIND"),    CT_STRFIND    },
            { _T("STRRFIND"),   CT_STRRFIND   },
            { _T("STRREPLACE"), CT_STRREPLACE },
            { _T("STRRPLC"),    CT_STRREPLACE },
            { _T("STRFROMHEX"), CT_STRFROMHEX },
            { _T("STRTOHEX"),   CT_STRTOHEX   }
        };

        NppExecHelpers::StrUpper(m_param);

        for ( const tCalcType& ct : arrCalcType )
        {
            if ( m_param == ct.szCalcType )
            {
                m_calcType = ct.nCalcType;
                break;
            }
        }
    }

    switch ( m_calcType )
    {
        case CT_FPARSER:
            calcFParser(); // calc
            break;
        case CT_STRLEN:
        case CT_STRLENUTF8:
        case CT_STRLENSCI:
            calcStrLen();
            break;
        case CT_STRUPPER:
        case CT_STRLOWER:
            calcStrCase();
            break;
        case CT_SUBSTR:
            calcSubStr();
            break;
        case CT_STRFIND:
        case CT_STRRFIND:
            calcStrFind();
            break;
        case CT_STRREPLACE:
            calcStrRplc();
            break;
        case CT_STRFROMHEX:
            calcStrFromHex();
            break;
        case CT_STRTOHEX:
            calcStrToHex();
            break;
    }
}

void CNppExecMacroVars::StrCalc::calcFParser()
{
    tstr calcError;

    if ( g_fp.Calculate(m_pNppExec, m_varValue, calcError, m_varValue) )
    {
      
        Runtime::GetLogger().AddEx( _T("; fparser calc result: %s"), m_varValue.c_str() );
    
    }
    else
    {
        calcError.Insert( 0, _T("- fparser calc error: ") );
        m_pNppExec->GetConsole().PrintError( calcError.c_str() );
    }
}

void CNppExecMacroVars::StrCalc::calcStrLen()
{
    int len = 0;

    if ( *m_pVar )
    {
        if ( m_calcType == CT_STRLENSCI )
        {
            HWND hSci = m_pNppExec->GetScintillaHandle();
            int nSciCodePage = (int) ::SendMessage(hSci, SCI_GETCODEPAGE, 0, 0);
            if ( nSciCodePage == SC_CP_UTF8 )
                m_calcType = CT_STRLENUTF8;
            else
                m_calcType = CT_STRLEN;

            Runtime::GetLogger().AddEx( _T("; scintilla's encoding: %d"), nSciCodePage );
        }

        if ( m_calcType == CT_STRLEN )
        {
            len = GetStrUnsafeLength(m_pVar);
        }
        else
        {
            #ifdef UNICODE
                len = ::WideCharToMultiByte(CP_UTF8, 0, m_pVar, -1, NULL, 0, NULL, NULL);
            #else
                wchar_t* pwVar = SysUniConv::newMultiByteToUnicode(m_pVar);
                len = ::WideCharToMultiByte(CP_UTF8, 0, pwVar, -1, NULL, 0, NULL, NULL);
                delete [] pwVar;
            #endif

            --len; // without trailing '\0'
        }
    }

    TCHAR szNum[50];
    c_base::_tint2str( len, szNum );
    m_varValue = szNum;

    Runtime::GetLogger().AddEx( 
      _T("; strlen%s: %s"), 
      (m_calcType == CT_STRLEN) ? _T("") : _T("utf8"),
      m_varValue.c_str() 
    );

}

void CNppExecMacroVars::StrCalc::calcStrCase()
{
    if ( *m_pVar )
    {
        m_varValue = m_pVar;
        if ( m_calcType == CT_STRUPPER )
            NppExecHelpers::StrUpper(m_varValue);
        else
            NppExecHelpers::StrLower(m_varValue);

        Runtime::GetLogger().AddEx( 
          _T("; %s: %s"), 
          (m_calcType == CT_STRUPPER) ? _T("strupper") : _T("strlower"),
          m_varValue.c_str() 
        );

    }
}

void CNppExecMacroVars::StrCalc::calcSubStr()
{
    if ( *m_pVar )
    {
        m_pVar = get_param(m_pVar, m_param);
        if ( isDecNumChar(m_param.GetAt(0)) || 
             (m_param.GetAt(0) == _T('-') && isDecNumChar(m_param.GetAt(1))) )
        {
            int pos = c_base::_tstr2int( m_param.c_str() );
            if ( *m_pVar )
            {
                m_pVar = get_param(m_pVar, m_param);
                if ( isDecNumChar(m_param.GetAt(0)) ||
                     (m_param.GetAt(0) == _T('-')) )
                {
                    int len = lstrlen(m_pVar);
                    int count = (m_param == _T("-")) ? len : c_base::_tstr2int( m_param.c_str() );

                    if ( pos < 0 )
                    {
                        // get pos characters from the end of string
                        pos += len;
                        if ( pos < 0 )
                            pos = 0;
                    }
                    else
                    {
                        if ( pos > len )
                            count = 0;
                    }

                    if ( count < 0 )
                    {
                        count += (len - pos);
                        if ( count < 0 )
                            count = 0;
                    }

                    if ( count > len - pos )
                    {
                        count = len - pos;
                    }

                    if ( count == 0 )
                        m_varValue.Clear();
                    else
                        m_varValue.Copy(m_pVar + pos, count);

                    Runtime::GetLogger().AddEx( 
                      _T("; substr: %s"), 
                      m_varValue.c_str() 
                    );

                }
                else
                {
                    m_pNppExec->GetConsole().PrintError( _T("- failed to get 2nd parameter of SUBSTR: a number or \'-\' expected") );
                }
            }
            else
            {
                m_pNppExec->GetConsole().PrintError( _T("- failed to get 2nd parameter of SUBSTR") );
            }
        }
        else
        {
            m_pNppExec->GetConsole().PrintError( _T("- failed to get 1st parameter of SUBSTR: a number expected") );
        }
    }
    else
    {
        m_pNppExec->GetConsole().PrintError( _T("- failed to get 1st parameter of SUBSTR") );
    }
}

void CNppExecMacroVars::StrCalc::calcStrFind()
{
    CStrSplitT<TCHAR> args;

    const int n = args.SplitToArgs(m_pVar);
    if ( n == 2 )
    {
        const tstr& S = args.GetArg(0);
        const tstr& SFind = args.GetArg(1);
        int pos = (m_calcType == CT_STRFIND) ? S.Find(SFind) : S.RFind(SFind);

        TCHAR szNum[50];
        c_base::_tint2str( pos, szNum );
        m_varValue = szNum;

        Runtime::GetLogger().AddEx( 
          _T("; %s: %s"), 
          (m_calcType == CT_STRFIND) ? _T("strfind") : _T("strrfind"),
          m_varValue.c_str() 
        );

    }
    else if ( n < 2 )
    {
        m_pNppExec->GetConsole().PrintError( _T("- not enough STRFIND parameters given: 2 parameters expected") );
    }
    else
    {
        m_pNppExec->GetConsole().PrintError( _T("- too much STRFIND parameters given: 2 parameters expected") );
        m_pNppExec->GetConsole().PrintError( _T("- try to enclose the STRFIND parameters with quotes, e.g. \"s\" \"sfind\"") );
    }
}

void CNppExecMacroVars::StrCalc::calcStrRplc()
{
    CStrSplitT<TCHAR> args;

    const int n = args.SplitToArgs(m_pVar);
    if ( n == 3 )
    {
        const tstr& SFind = args.GetArg(1);
        const tstr& SReplace = args.GetArg(2);
        m_varValue = args.GetArg(0);
        m_varValue.Replace( SFind.c_str(), SReplace.c_str() );

        Runtime::GetLogger().AddEx( 
          _T("; strreplace: %s"), 
          m_varValue.c_str() 
        );

    }
    else if ( n < 3 )
    {
        m_pNppExec->GetConsole().PrintError( _T("- not enough STRREPLACE parameters given: 3 parameters expected") );
    }
    else
    {
        m_pNppExec->GetConsole().PrintError( _T("- too much STRREPLACE parameters given: 3 parameters expected") );
        m_pNppExec->GetConsole().PrintError( _T("- try to enclose the STRREPLACE parameters with quotes, e.g. \"s\" \"sfind\" \"sreplace\"") );
    }
}

void CNppExecMacroVars::StrCalc::calcStrFromHex()
{
    if ( *m_pVar )
    {
        tstr hexStr = m_pVar;
        m_varValue.Reserve(hexStr.length());
        int nBytes = c_base::_thexstrex2buf(hexStr.c_str(), (c_base::byte_t *)(m_varValue.c_str()), m_varValue.GetMemSize());
        if ( nBytes % sizeof(TCHAR) != 0 )
        {
            ((c_base::byte_t *)(m_varValue.c_str()))[nBytes] = 0;
            ++nBytes;
        }
        m_varValue.SetLengthValue(nBytes / sizeof(TCHAR));

        Runtime::GetLogger().AddEx( 
          _T("; strfromhex: %s"), 
          m_varValue.c_str() 
        );

    }
}

void CNppExecMacroVars::StrCalc::calcStrToHex()
{
    if ( *m_pVar )
    {
        tstr Str = m_pVar;
        NppExecHelpers::StrUnquote(Str);

        m_varValue.Reserve(1 + Str.length() * sizeof(TCHAR) * 3);
        int nLen = c_base::_tbuf2hexstr((const c_base::byte_t *)(Str.c_str()), Str.length()*sizeof(TCHAR), 
                                        m_varValue.c_str(), m_varValue.GetMemSize(),
                                       _T(" "));
        m_varValue.SetLengthValue(nLen);

        Runtime::GetLogger().AddEx( 
          _T("; strtohex: %s"), 
          m_varValue.c_str() 
        );

    }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK nppPluginWndProc(HWND hWnd, UINT uMessage, WPARAM wParam, LPARAM lParam)
{
  if (uMessage == WM_CLOSE)
  {
    Runtime::GetLogger().Add/*_WithoutOutput*/( _T("; WM_CLOSE from Notepad++") );
      
    CNppExecCommandExecutor& CommandExecutor = Runtime::GetNppExec().GetCommandExecutor();

    if (CommandExecutor.IsChildProcessRunning() || CommandExecutor.IsScriptRunningOrQueued())
    {
        CNppExecCommandExecutor::ScriptableCommand * pCommand = new CNppExecCommandExecutor::NppExitCommand(tstr());
        CommandExecutor.ExecuteCommand(pCommand);
        return 0;
    }
  }

#ifdef UNICODE
  return ::CallWindowProcW(nppOriginalWndProc, hWnd, uMessage, wParam, lParam);
#else
  return ( g_bIsNppUnicode ?
             ::CallWindowProcW(nppOriginalWndProc, hWnd, uMessage, wParam, lParam) :
                 ::CallWindowProcA(nppOriginalWndProc, hWnd, uMessage, wParam, lParam) );
#endif
}


int FileFilterPos(const TCHAR* szFilePath)
{
  if ( !szFilePath )
    return -1;

  const TCHAR* p = szFilePath;
  while ( *p )
  {
    if ( (*p == _T('*')) || (*p == _T('?')) )
      return ( (int) (p - szFilePath) );
    ++p;
  }
  return -1;
}

void GetPathAndFilter(const TCHAR* szPathAndFilter, 
       int nFilterPos, tstr& out_Path, tstr& out_Filter)
{
  TCHAR path[FILEPATH_BUFSIZE];

  out_Path.Clear();
  int pos = nFilterPos;
  while ( --pos >= 0 )
  {
    if ( (szPathAndFilter[pos] == _T('\\')) || 
         (szPathAndFilter[pos] == _T('/')) )
    {
      out_Path.Copy( szPathAndFilter, pos );
      break;
    }
  }

  if ( (nFilterPos == 1) && 
       (szPathAndFilter[0] == _T('\\') || szPathAndFilter[0] == _T('/')) )
  {
    out_Path.Copy( szPathAndFilter, 1 );
  }
  
  if ( nFilterPos >= 0)
  {
    if ( pos < 0 )  pos = -1;
    out_Filter.Copy( szPathAndFilter + 1 + pos );
  }
  else
  {
    out_Path = szPathAndFilter;
    out_Filter = _T("*");
  }
  
  if ( out_Path.IsEmpty() )
  {
    path[0] = 0;
    GetCurrentDirectory( FILEPATH_BUFSIZE - 1, path );
    out_Path = path;
  }
  else if ( !isFullPath(out_Path) )
  {
    // is not full path
    path[0] = 0;
    GetCurrentDirectory( FILEPATH_BUFSIZE - 1, path );
    if ( (out_Path.GetAt(0) == _T('\\') && out_Path.GetAt(1) != _T('\\')) || 
         (out_Path.GetAt(0) == _T('/') && out_Path.GetAt(1) != _T('/')) )
    {
      path[2] = 0;
    }
    else
    {
      int len = lstrlen( path );
      if ( path[len - 1] != _T('\\') )
      {
        path[len++] = _T('\\');
        path[len] = 0;
      }
    }
    out_Path.Insert( 0, path );
  }
}

int FilterOK(const TCHAR* mask, const TCHAR* str)
{
  tstr S = str;
  NppExecHelpers::StrLower(S);
  return c_base::_tmatch_mask( mask, S.c_str() );
}

void GetFilePathNamesList(const TCHAR* szPath, const TCHAR* szFilter, 
       CListT<tstr>& FilesList)
{
    FilesList.DeleteAll();
  
    if ( (!szPath) || (szPath[0] == 0) )
        return;

    tstr S = szPath;
    if ( S.GetLastChar() != _T('\\') )  S += _T('\\');
    S += _T("*.*");

    CDirFileLister FileLst;

    if ( !FileLst.FindNext(S.c_str(), 
            CDirFileLister::ESF_FILES | CDirFileLister::ESF_SORTED) )
        return;
  
    tstr  Fltr;
    tstr  FilePathName;
    bool  bFilter = (szFilter && (szFilter[0] != 0)) ? true : false;
    if ( bFilter )
    {
        Fltr = szFilter;
        NppExecHelpers::StrLower(Fltr);
    }
    S.SetSize( S.length() - 3 ); // removing last "*.*"

    const TCHAR* pszFileName;
    unsigned int len;
    
    do
    {
        pszFileName = FileLst.GetItem(&len);
        if ( (!bFilter) || FilterOK(Fltr.c_str(), pszFileName) )
        {
            FilePathName = S;
            FilePathName.Append( pszFileName, len );
            FilesList.Add( FilePathName );
        }
    }
    while ( FileLst.GetNext() );
  
}

bool PrintDirContent(CNppExec* pNppExec, const TCHAR* szPath, const TCHAR* szFilter)
{
    if ( (!szPath) || (szPath[0] == 0) )
        return false;

    tstr S = szPath;
    if ( S.GetLastChar() != _T('\\') )  S += _T('\\');
    S += _T("*.*");

    CDirFileLister DirFileLst;

    if ( !DirFileLst.FindNext(S.c_str(), 
            CDirFileLister::ESF_DIRS | CDirFileLister::ESF_FILES | 
              CDirFileLister::ESF_PLACEDIRSFIRST | CDirFileLister::ESF_SORTED) )
        return false;

    tstr  Fltr;
    bool  bFilter = (szFilter && (szFilter[0] != 0)) ? true : false;
    if ( bFilter )
    {
        Fltr = szFilter;
        NppExecHelpers::StrLower(Fltr);
    }

    const TCHAR* pszFileName;
    unsigned int len;
    bool         isDir;

    do
    {
        pszFileName = DirFileLst.GetItem(&len, &isDir);
        if ( (!bFilter) || FilterOK(Fltr.c_str(), pszFileName) )
        {
            if ( isDir )
            {
                S = _T("<DIR> ");
                S.Append( pszFileName, len );
                pszFileName = S.c_str();
            }
            pNppExec->GetConsole().PrintStr( pszFileName, true );
        }
    }
    while ( DirFileLst.GetNext() );

    return true;
}

void runInputBox(CScriptEngine* pScriptEngine, const TCHAR* szMessage)
{
    const int MAX_VAR_FIELDS = 20;

    TCHAR szVarFldName[40];
    tstr  varName;
    tstr  S;
    CNppExec* pNppExec = pScriptEngine->GetNppExec();

    Runtime::GetLogger().Add(   _T("runInputBox()") );
    Runtime::GetLogger().Add(   _T("{") );
    Runtime::GetLogger().IncIndentLevel();
    Runtime::GetLogger().AddEx( _T("[in]  %s"), szMessage );
    
    // init the InputBox dialog values
    InputBoxDlg.m_InputMessage = szMessage;
    InputBoxDlg.m_InputVarName = MACRO_INPUT;
    InputBoxDlg.m_InputVarName += _T(" =");
    InputBoxDlg.setInputBoxType(CInputBoxDlg::IBT_INPUTBOX);
    
    if ( CNppExec::_bIsNppShutdown )
    {
        Runtime::GetLogger().Add(   _T("; InputBox suppressed as Notepad++ is exiting") );

        InputBoxDlg.m_OutputValue.Clear();
    }
    else
    {
        // show the InputBox dialog
        pNppExec->PluginDialogBox(IDD_INPUTBOX, InputBoxDlgProc);
    }

    Runtime::GetLogger().Add(   _T("; InputBox returned") );

    // preparing the output value
    pNppExec->GetMacroVars().CheckAllMacroVars(pScriptEngine, InputBoxDlg.m_OutputValue, true);

    // removing previous values
    varName = MACRO_INPUT;
    pNppExec->GetMacroVars().SetUserMacroVar(pScriptEngine, varName, _T(""), CNppExecMacroVars::svRemoveVar | CNppExecMacroVars::svLocalVar); // local var
    for ( int i = 1; i <= MAX_VAR_FIELDS; i++ )
    {
        wsprintf(szVarFldName, MACRO_INPUTFMT, i);
        varName = szVarFldName;
        pNppExec->GetMacroVars().SetUserMacroVar(pScriptEngine, varName, _T(""), CNppExecMacroVars::svRemoveVar | CNppExecMacroVars::svLocalVar); // local var
    }
    
    // getting new values
    CStrSplitT<TCHAR> fields;
    int nFields = fields.SplitToArgs(InputBoxDlg.m_OutputValue, MAX_VAR_FIELDS);

    // setting new values
    varName = MACRO_INPUT;
    pNppExec->GetMacroVars().SetUserMacroVar(pScriptEngine, varName, InputBoxDlg.m_OutputValue, CNppExecMacroVars::svLocalVar); // local var
    S = _T("local ");
    S += varName;
    S += _T(" = ");
    S += InputBoxDlg.m_OutputValue;
    pNppExec->GetConsole().PrintMessage( S.c_str(), true );

    Runtime::GetLogger().AddEx( _T("[out] %s"), S.c_str() );

    for ( int i = 1; i <= nFields; i++ )
    {
        wsprintf(szVarFldName, MACRO_INPUTFMT, i);
        varName = szVarFldName;
        pNppExec->GetMacroVars().SetUserMacroVar(pScriptEngine, varName, fields.GetArg(i - 1), CNppExecMacroVars::svLocalVar); // local var
        S = _T("local ");
        S += varName;
        S += _T(" = ");
        S += fields.GetArg(i - 1);
        pNppExec->GetConsole().PrintMessage( S.c_str(), true );

        Runtime::GetLogger().AddEx( _T("[out] %s"), S.c_str() );

    }

    Runtime::GetLogger().DecIndentLevel();
    Runtime::GetLogger().Add(   _T("}") );
    
    
    // restore the focus
    if ( (pNppExec->m_hFocusedWindowBeforeScriptStarted == pNppExec->GetConsole().GetConsoleWnd()) ||
         (pNppExec->m_hFocusedWindowBeforeScriptStarted == pNppExec->GetConsole().GetDialogWnd()) )
    {
        if ( ::IsWindowVisible(pNppExec->GetConsole().GetDialogWnd()) )
        {
            pNppExec->SendNppMsg(NPPM_DMMSHOW, 0, (LPARAM) pNppExec->GetConsole().GetDialogWnd());
        }
    }
}

char* SciTextFromLPCTSTR(LPCTSTR pText, HWND hSci, int* pnLen )
{
    char* pOutSciText = NULL;
    int   nSciCodePage = (int) ::SendMessage(hSci, SCI_GETCODEPAGE, 0, 0);

    switch ( nSciCodePage )
    {
        case 0:          // ANSI
            #ifdef UNICODE
                pOutSciText = SysUniConv::newUnicodeToMultiByte(pText, -1, CP_ACP, pnLen);
            #else
                pOutSciText = (char *) pText;
            #endif
            break;

        case SC_CP_UTF8: // UTF-8
            #ifdef UNICODE
                pOutSciText = SysUniConv::newUnicodeToUTF8(pText, -1, pnLen);
            #else
                pOutSciText = SysUniConv::newMultiByteToUTF8(pText, -1, CP_ACP);
            #endif
            break;

        default:         // multi-byte encoding
            #ifdef UNICODE
                pOutSciText = SysUniConv::newUnicodeToMultiByte(pText, -1, CP_ACP, pnLen);
            #else
                pOutSciText = (char *) pText;
            #endif
            break;
    }

    return pOutSciText;
}

LPTSTR SciTextToLPTSTR(const char* pSciText, HWND hSci)
{
    LPTSTR pOutText = NULL;
    int    nSciCodePage = (int) ::SendMessage(hSci, SCI_GETCODEPAGE, 0, 0);

    switch ( nSciCodePage )
    {
        case SC_CP_UTF8:
            #ifdef UNICODE
                pOutText = SysUniConv::newUTF8ToUnicode(pSciText);
            #else
                pOutText = SysUniConv::newUTF8ToMultiByte(pSciText, -1, CP_ACP);
            #endif
            break;

        default:
            #ifdef UNICODE
                pOutText = SysUniConv::newMultiByteToUnicode(pSciText, -1, CP_ACP);
            #else
                pOutText = (char *) pSciText;
            #endif
            break;
    }

    return pOutText;
}
