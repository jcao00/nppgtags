/**
 *  \file
 *  \brief  GTags result Scintilla view UI
 *
 *  \author  Pavel Nedev <pg.nedev@gmail.com>
 *
 *  \section COPYRIGHT
 *  Copyright(C) 2014 Pavel Nedev
 *
 *  \section LICENSE
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#pragma comment (lib, "comctl32")


#include "ScintillaViewUI.h"
#include "INpp.h"
#include "DBManager.h"
#include "DocLocation.h"
#include <commctrl.h>


// Scintilla user defined styles IDs
enum
{
    SCE_GTAGS_HEADER = 151,
    SCE_GTAGS_PROJECT_PATH,
    SCE_GTAGS_FILE,
    SCE_GTAGS_WORD2SEARCH
};


// Scintilla fold levels
enum
{
    SEARCH_HEADER_LVL = SC_FOLDLEVELBASE + 1,
    FILE_HEADER_LVL,
    RESULT_LVL
};


const TCHAR ScintillaViewUI::cClassName[] = _T("ScintillaViewUI");


using namespace GTags;


/**
 *  \brief
 */
ScintillaViewUI::Tab::Tab(const GTags::CmdData& cmd) :
    _uiLine(1), _uiFoldLine(0)
{
    _cmdID = cmd.GetID();

    Tools::WtoA(_projectPath, _countof(_projectPath), cmd.GetDBPath());
    Tools::WtoA(_search, _countof(_search), cmd.GetTag());

    // Add the search header - cmd name + search word + project path
    _uiBuf = cmd.GetName();
    _uiBuf += " \"";
    _uiBuf += _search;
    _uiBuf += "\" in \"";
    _uiBuf += _projectPath;
    _uiBuf += "\"";

    // parsing result buffer and composing UI buffer
    if (_cmdID == FIND_FILE)
        parseFindFile(_uiBuf, cmd.GetResult());
    else
        parseCmd(_uiBuf, cmd.GetResult());
}


/**
 *  \brief
 */
ScintillaViewUI::Tab::~Tab()
{
}


/**
 *  \brief
 */
void ScintillaViewUI::Tab::parseCmd(CTextA& dst, const char* src)
{
    unsigned tagLen = strlen(_search);
    const char* lineRes;
    const char* lineResEnd;
    const char* fileResEnd;
    const char* prevFile = NULL;
    unsigned prevFileLen = 0;

    for (;;)
    {
        while (*src == '\n' || *src == '\r' || *src == ' ' || *src == '\t')
            src++;
        if (*src == 0) break;

        src += tagLen; // skip search word from result buffer
        while (*src == ' ' || *src == '\t')
            src++;

        lineRes = lineResEnd = src;
        while (*lineResEnd != ' ' && *lineResEnd != '\t')
            lineResEnd++;

        src = lineResEnd;
        while (*src == ' ' || *src == '\t')
            src++;

        fileResEnd = src;
        while (*fileResEnd != ' ' && *fileResEnd != '\t')
            fileResEnd++;

        // add new file name to the UI buffer only if it is different
        // than the previous one
        if (prevFile == NULL || (unsigned)(fileResEnd - src) != prevFileLen ||
            strncmp(src, prevFile, prevFileLen))
        {
            prevFile = src;
            prevFileLen = fileResEnd - src;
            dst += "\n\t";
            dst.append(prevFile, prevFileLen);
        }

        dst += "\n\t\tline ";
        dst.append(lineRes, lineResEnd - lineRes);
        dst += ":\t";

        src = fileResEnd;
        while (*src == ' ' || *src == '\t')
            src++;

        lineResEnd = src;
        while (*lineResEnd != '\n' && *lineResEnd != '\r')
            lineResEnd++;

        dst.append(src, lineResEnd - src);
        src = lineResEnd;
    }
}


/**
 *  \brief
 */
void ScintillaViewUI::Tab::parseFindFile(CTextA& dst, const char* src)
{
    const char* eol;

    for (;;)
    {
        while (*src == '\n' || *src == '\r' || *src == ' ' || *src == '\t')
            src++;
        if (*src == 0) break;

        eol = src;
        while (*eol != '\n' && *eol != '\r' && *eol != 0)
            eol++;

        dst += "\n\t";
        dst.append(src, eol - src);
        src = eol;
    }
}


/**
 *  \brief
 */
int ScintillaViewUI::Register()
{
    if (_hWnd)
        return 0;

    WNDCLASS wc         = {0};
    wc.style            = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc      = wndProc;
    wc.hInstance        = HMod;
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground    = GetSysColorBrush(COLOR_WINDOW);
    wc.lpszClassName    = cClassName;

    RegisterClass(&wc);

    INITCOMMONCONTROLSEX icex   = {0};
    icex.dwSize                 = sizeof(icex);
    icex.dwICC                  = ICC_STANDARD_CLASSES;

    InitCommonControlsEx(&icex);

    if (composeWindow() == NULL)
        return -1;

    tTbData data        = {0};
    data.hClient        = _hWnd;
    data.pszName        = const_cast<TCHAR*>(cPluginName);
    data.uMask          = 0;
    data.pszAddInfo     = NULL;
    data.uMask          = DWS_DF_CONT_BOTTOM;
    data.pszModuleName  = DllPath.GetFilename();
    data.dlgID          = 0;

    INpp& npp = INpp::Get();
    npp.RegisterWin(_hWnd);
    npp.RegisterDockingWin(data);
    npp.HideDockingWin(_hWnd);

    return 0;
}


/**
 *  \brief
 */
void ScintillaViewUI::Unregister()
{
    if (_hWnd == NULL)
        return;

    closeAllTabs();

    INpp& npp = INpp::Get();

    if (_hSci)
    {
        npp.DestroySciHandle(_hSci);
        _hSci = NULL;
    }

    npp.UnregisterWin(_hWnd);
    SendMessage(_hWnd, WM_CLOSE, 0, 0);
    _hWnd = NULL;

    UnregisterClass(cClassName, HMod);
}


/**
 *  \brief
 */
void ScintillaViewUI::Show(const CmdData& cmd)
{
    if (_hWnd == NULL)
        return;

    INpp& npp = INpp::Get();

    if (cmd.GetResultLen() > 262144) // 256k
    {
        TCHAR buf[512];
        _sntprintf_s(buf, _countof(buf), _TRUNCATE,
                _T("%s \"%s\": A lot of matches were found, ")
                _T("parsing those will be rather slow.\n")
                _T("Are you sure you want to proceed?"),
                cmd.GetName(), cmd.GetTag());
        int choice = MessageBox(npp.GetHandle(), buf, cPluginName,
                MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
        if (choice != IDYES)
            return;
    }

    // parsing results happens here
    Tab* tab = new Tab(cmd);

    AUTOLOCK(_lock);

    int i;
    for (i = TabCtrl_GetItemCount(_hTab); i; i--)
    {
        Tab* oldTab = getTab(i - 1);
        if (oldTab && *tab == *oldTab) // same search tab already present?
        {
            if (_activeTab == oldTab) // is this the currently active tab?
                _activeTab = NULL;
            delete oldTab;
            break;
        }
    }

    if (i == 0) // search is completely new - add new tab
    {
        TCITEM tci  = {0};
        tci.mask    = TCIF_TEXT | TCIF_PARAM;
        tci.pszText = const_cast<TCHAR*>(cmd.GetTag());
        tci.lParam  = (LPARAM)tab;

        i = TabCtrl_InsertItem(_hTab, TabCtrl_GetItemCount(_hTab), &tci);
        if (i == -1)
        {
            delete tab;
            return;
        }
    }
    else // same search tab exists - reuse it, just update results
    {
        TCITEM tci  = {0};
        tci.mask    = TCIF_PARAM;
        tci.lParam  = (LPARAM)tab;

        if (!TabCtrl_SetItem(_hTab, --i, &tci))
        {
            TabCtrl_DeleteItem(_hTab, i);
            delete tab;
            closeAllTabs();
            return;
        }
    }

    TabCtrl_SetCurSel(_hTab, i);
    loadTab(tab);

    npp.UpdateDockingWin(_hWnd);
    npp.ShowDockingWin(_hWnd);
    SetFocus(_hSci);
}


/**
 *  \brief
 */
void ScintillaViewUI::ResetStyle()
{
    if (_hWnd == NULL)
        return;

    INpp& npp = INpp::Get();

    char font[32];
    npp.GetFontName(font, _countof(font));
    int size = npp.GetFontSize();

    sendSci(SCI_STYLERESETDEFAULT);
    setStyle(STYLE_DEFAULT, cBlack, cWhite, false, false, size, font);
    sendSci(SCI_STYLECLEARALL);

    setStyle(SCE_GTAGS_HEADER, cBlack, RGB(179,217,217), true);
    setStyle(SCE_GTAGS_PROJECT_PATH, cBlack, RGB(179,217,217), true, true);
    setStyle(SCE_GTAGS_FILE, cBlue, cWhite, true);
    setStyle(SCE_GTAGS_WORD2SEARCH, cRed, cWhite, true);
}


/**
 *  \brief
 */
void ScintillaViewUI::setStyle(int style, COLORREF fore, COLORREF back,
        bool bold, bool italic, int size, const char *font)
{
    sendSci(SCI_STYLESETEOLFILLED, style, 1);
    sendSci(SCI_STYLESETFORE, style, fore);
    sendSci(SCI_STYLESETBACK, style, back);
    sendSci(SCI_STYLESETBOLD, style, bold);
    sendSci(SCI_STYLESETITALIC, style, italic);
    if (size >= 1)
        sendSci(SCI_STYLESETSIZE, style, size);
    if (font)
        sendSci(SCI_STYLESETFONT, style, reinterpret_cast<LPARAM>(font));
}


/**
 *  \brief
 */
void ScintillaViewUI::configScintilla()
{
    sendSci(SCI_SETCODEPAGE, SC_CP_UTF8);
    sendSci(SCI_SETEOLMODE, SC_EOL_CRLF);
    sendSci(SCI_USEPOPUP, false);
    sendSci(SCI_SETUNDOCOLLECTION, false);
    sendSci(SCI_SETCURSOR, SC_CURSORARROW);
    sendSci(SCI_SETCARETSTYLE, CARETSTYLE_INVISIBLE);
    sendSci(SCI_SETCARETLINEBACK, RGB(222,222,238));
    sendSci(SCI_SETCARETLINEVISIBLE, 1);
    sendSci(SCI_SETCARETLINEVISIBLEALWAYS, 1);

    sendSci(SCI_SETLAYOUTCACHE, SC_CACHE_DOCUMENT);

    // Implement lexer in the container
    sendSci(SCI_SETLEXER, 0);

    ResetStyle();

    sendSci(SCI_SETPROPERTY, reinterpret_cast<WPARAM>("fold"),
            reinterpret_cast<LPARAM>("1"));

    sendSci(SCI_SETMARGINTYPEN, 1, SC_MARGIN_SYMBOL);
    sendSci(SCI_SETMARGINMASKN, 1, SC_MASK_FOLDERS);
    sendSci(SCI_SETMARGINWIDTHN, 1, 20);
    sendSci(SCI_SETFOLDMARGINCOLOUR, 1, cBlack);
    sendSci(SCI_SETFOLDMARGINHICOLOUR, 1, cBlack);
    sendSci(SCI_SETFOLDFLAGS, 0);
    sendSci(SCI_SETAUTOMATICFOLD, SC_AUTOMATICFOLD_SHOW |
            SC_AUTOMATICFOLD_CLICK | SC_AUTOMATICFOLD_CHANGE);

    sendSci(SCI_MARKERDEFINE, SC_MARKNUM_FOLDER, SC_MARK_BOXPLUS);
    sendSci(SCI_MARKERDEFINE, SC_MARKNUM_FOLDEROPEN, SC_MARK_BOXMINUS);
    sendSci(SCI_MARKERDEFINE, SC_MARKNUM_FOLDEREND, SC_MARK_BOXPLUSCONNECTED);
    sendSci(SCI_MARKERDEFINE, SC_MARKNUM_FOLDERSUB, SC_MARK_VLINE);
    sendSci(SCI_MARKERDEFINE, SC_MARKNUM_FOLDERTAIL, SC_MARK_LCORNER);
    sendSci(SCI_MARKERDEFINE, SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_TCORNER);
    sendSci(SCI_MARKERDEFINE, SC_MARKNUM_FOLDEROPENMID,
            SC_MARK_BOXMINUSCONNECTED);
}


/**
 *  \brief
 */
HWND ScintillaViewUI::composeWindow()
{
    INpp& npp = INpp::Get();
    HWND hOwner = npp.GetHandle();
    RECT win;
    GetWindowRect(hOwner, &win);
    DWORD style = WS_POPUP | WS_CAPTION | WS_SIZEBOX;
    _hWnd = CreateWindow(cClassName, cPluginName,
            style, win.left, win.top,
            win.right - win.left, win.bottom - win.top,
            hOwner, NULL, HMod, (LPVOID)this);
    if (_hWnd == NULL)
        return NULL;

    _hSci = npp.CreateSciHandle(_hWnd);
    if (_hSci)
    {
        _sciFunc =
                (SciFnDirect)::SendMessage(_hSci, SCI_GETDIRECTFUNCTION, 0, 0);
        _sciPtr = (sptr_t)::SendMessage(_hSci, SCI_GETDIRECTPOINTER, 0, 0);
    }
    if (_hSci == NULL || _sciFunc == NULL || _sciPtr == NULL)
    {
        SendMessage(_hWnd, WM_CLOSE, 0, 0);
        _hWnd = NULL;
        _hSci = NULL;
        return NULL;
    }

    AdjustWindowRect(&win, style, FALSE);
    MoveWindow(_hWnd, win.left, win.top,
            win.right - win.left, win.bottom - win.top, TRUE);
    GetClientRect(_hWnd, &win);

    _hTab = CreateWindowEx(0, WC_TABCONTROL, _T("TabCtrl"),
            WS_CHILD | WS_VISIBLE | TCS_BUTTONS | TCS_FOCUSNEVER,
            0, 0, win.right - win.left, win.bottom - win.top,
            _hWnd, NULL, HMod, NULL);

    TabCtrl_SetExtendedStyle(_hTab, TCS_EX_FLATSEPARATORS);

    TabCtrl_AdjustRect(_hTab, FALSE, &win);
    MoveWindow(_hSci, win.left, win.top,
            win.right - win.left, win.bottom - win.top, TRUE);

    configScintilla();

    ShowWindow(_hSci, SW_SHOWNORMAL);

    return _hWnd;
}


/**
 *  \brief
 */
ScintillaViewUI::Tab* ScintillaViewUI::getTab(int i)
{
    if (i == -1)
    {
        i = TabCtrl_GetCurSel(_hTab);
        if (i == -1)
            return NULL;
    }

    TCITEM tci  = {0};
    tci.mask    = TCIF_PARAM;

    if (!TabCtrl_GetItem(_hTab, i, &tci))
        return NULL;

    return (Tab*)tci.lParam;
}


/**
 *  \brief
 */
void ScintillaViewUI::loadTab(ScintillaViewUI::Tab* tab)
{
    // store current view if there is one
    if (_activeTab)
    {
        _activeTab->_uiLine =
                sendSci(SCI_LINEFROMPOSITION, sendSci(SCI_GETCURRENTPOS));
        _activeTab->_uiFoldLine =
                sendSci(SCI_GETFOLDPARENT, _activeTab->_uiLine);
    }

    _activeTab = tab;

    sendSci(SCI_SETREADONLY, 0);
    sendSci(SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(tab->_uiBuf.C_str()));
    sendSci(SCI_SETREADONLY, 1);

    sendSci(SCI_SETFIRSTVISIBLELINE, tab->_uiLine > 3 ? tab->_uiLine - 3 : 0);
    const int pos = sendSci(SCI_POSITIONFROMLINE, tab->_uiLine);
    sendSci(SCI_SETSEL, pos, pos);

    SetFocus(_hSci);
}


/**
 *  \brief
 */
bool ScintillaViewUI::openItem(int lineNum)
{
    int lineLen = sendSci(SCI_LINELENGTH, lineNum);
    char* lineTxt = new char[lineLen + 1];

    sendSci(SCI_GETLINE, lineNum, reinterpret_cast<LPARAM>(lineTxt));

    long line = -1;
    int i;

    if (lineTxt[1] == '\t')
    {
        for (i = 7; lineTxt[i] != ':'; i++);
        lineTxt[i] = 0;
        line = atoi(&lineTxt[7]) - 1;
        delete [] lineTxt;

        lineNum = sendSci(SCI_GETFOLDPARENT, lineNum);
        if (lineNum == -1)
            return false;

        lineLen = sendSci(SCI_LINELENGTH, lineNum);
        lineTxt = new char[lineLen + 1];
        sendSci(SCI_GETLINE, lineNum, reinterpret_cast<LPARAM>(lineTxt));
    }

    lineTxt[lineLen] = 0;
    for (i = 1; lineTxt[i] != '\r' && lineTxt[i] != '\n'; i++);
    lineTxt[i] = 0;

    CPath file(_activeTab->_projectPath);
    CText str(&lineTxt[1]);
	file += str.C_str();
    delete [] lineTxt;

    INpp& npp = INpp::Get();
    if (!file.FileExists())
    {
        MessageBox(npp.GetHandle(),
                _T("File not found, update database and search again"),
                cPluginName, MB_OK | MB_ICONEXCLAMATION);
        return true;
    }

    DocLocation::Get().Push();
    npp.OpenFile(file.C_str());
    SetFocus(npp.ReadSciHandle());

    // GTags command is FIND_FILE
    if (line == -1)
    {
        npp.ClearSelection();
        return true;
    }

    bool wholeWord =
            (_activeTab->_cmdID != GREP && _activeTab->_cmdID != FIND_LITERAL);

    if (!npp.SearchText(_activeTab->_search, true, wholeWord,
            npp.PositionFromLine(line), npp.LineEndPosition(line)))
    {
        MessageBox(npp.GetHandle(),
                _T("Look-up mismatch, update database and search again"),
                cPluginName, MB_OK | MB_ICONINFORMATION);
    }

    return true;
}


/**
 *  \brief
 */
void ScintillaViewUI::styleString(int styleID, const char* str,
        int lineNum, int lineOffset, bool matchCase, bool wholeWord)
{
    struct TextToFind ttf = {0};
    ttf.lpstrText = const_cast<char*>(str);
    ttf.chrg.cpMin = sendSci(SCI_POSITIONFROMLINE, lineNum) + lineOffset;
    ttf.chrg.cpMax = sendSci(SCI_GETLINEENDPOSITION, lineNum);

    int searchFlags = 0;
    if (matchCase)
        searchFlags |= SCFIND_MATCHCASE;
    if (wholeWord)
        searchFlags |= SCFIND_WHOLEWORD;

    if (sendSci(SCI_FINDTEXT, searchFlags,
            reinterpret_cast<LPARAM>(&ttf)) != -1)
    {
        sendSci(SCI_STARTSTYLING, ttf.chrgText.cpMin, 0xFF);
        sendSci(SCI_SETSTYLING, ttf.chrgText.cpMax - ttf.chrgText.cpMin,
                styleID);
    }
}


/**
 *  \brief
 */
void ScintillaViewUI::onStyleNeeded(SCNotification* notify)
{
    if (_activeTab == NULL)
        return;

    int lineNum = sendSci(SCI_LINEFROMPOSITION,
            sendSci(SCI_GETENDSTYLED));
    const int endPos = notify->position;

    for (int startPos = sendSci(SCI_POSITIONFROMLINE, lineNum);
        endPos > startPos;
        startPos = sendSci(SCI_POSITIONFROMLINE, ++lineNum))
    {
        int lineLen = sendSci(SCI_LINELENGTH, lineNum);
        if (lineLen == 0)
            continue;

        if ((char)sendSci(SCI_GETCHARAT, startPos) != '\t')
        {
            sendSci(SCI_STARTSTYLING, startPos, 0xFF);
            sendSci(SCI_SETSTYLING, lineLen, SCE_GTAGS_HEADER);

            int pathLen = strlen(_activeTab->_projectPath);
            startPos = sendSci(SCI_GETLINEENDPOSITION, lineNum) - pathLen - 1;

            sendSci(SCI_STARTSTYLING, startPos, 0xFF);
            sendSci(SCI_SETSTYLING, pathLen, SCE_GTAGS_PROJECT_PATH);
            sendSci(SCI_SETFOLDLEVEL, lineNum,
                    SEARCH_HEADER_LVL | SC_FOLDLEVELHEADERFLAG);
        }
        else
        {
            if ((char)sendSci(SCI_GETCHARAT, startPos + 1) != '\t')
            {
                sendSci(SCI_STARTSTYLING, startPos, 0xFF);
                sendSci(SCI_SETSTYLING, lineLen, SCE_GTAGS_FILE);
                if (_activeTab->_cmdID == FIND_FILE)
                {
                    styleString(SCE_GTAGS_WORD2SEARCH,
                            _activeTab->_search, lineNum);
                    sendSci(SCI_SETFOLDLEVEL, lineNum, RESULT_LVL);
                }
                else
                {
                    sendSci(SCI_SETFOLDLEVEL, lineNum,
                            FILE_HEADER_LVL | SC_FOLDLEVELHEADERFLAG);
                    if (lineNum != _activeTab->_uiFoldLine)
                        sendSci(SCI_FOLDLINE, lineNum, SC_FOLDACTION_CONTRACT);
                }
            }
            else
            {
                bool wholeWord = (_activeTab->_cmdID == GREP ||
                        _activeTab->_cmdID == FIND_LITERAL) ? false : true;
                styleString(SCE_GTAGS_WORD2SEARCH,
                        _activeTab->_search, lineNum, 7, true, wholeWord);
                sendSci(SCI_SETFOLDLEVEL, lineNum, RESULT_LVL);
            }
        }
    }
}


/**
 *  \brief
 */
void ScintillaViewUI::onDoubleClick(SCNotification* notify)
{
    if (!_lock.TryLock())
        return;

    int pos = notify->position;
    int lineNum = sendSci(SCI_LINEFROMPOSITION, pos);

    if (lineNum == 0)
    {
        pos = sendSci(SCI_GETCURRENTPOS);
        if (pos == sendSci(SCI_POSITIONAFTER, pos)) // end of document
        {
            lineNum = sendSci(SCI_LINEFROMPOSITION, pos);
            int foldLine = sendSci(SCI_GETFOLDPARENT, lineNum);
            if (!sendSci(SCI_GETFOLDEXPANDED, foldLine))
            {
                lineNum = foldLine;
                pos = sendSci(SCI_POSITIONFROMLINE, lineNum);
            }
        }
        else
        {
            lineNum = sendSci(SCI_LINEFROMPOSITION, pos);
        }
    }

    // Clear double-click auto-selection
    sendSci(SCI_SETSEL, pos, pos);

    if (sendSci(SCI_LINELENGTH, lineNum))
    {
        if (sendSci(SCI_GETFOLDLEVEL, lineNum) & SC_FOLDLEVELHEADERFLAG)
            sendSci(SCI_TOGGLEFOLD, lineNum);
        else
            openItem(lineNum);
    }

    _lock.Unlock();
}


/**
 *  \brief
 */
void ScintillaViewUI::onCharAddTry(SCNotification* notify)
{
    if (!_lock.TryLock())
        return;

    if (notify->ch == ' ')
    {
        const int lineNum =
                sendSci(SCI_LINEFROMPOSITION, sendSci(SCI_GETCURRENTPOS));
        if (sendSci(SCI_LINELENGTH, lineNum))
        {
            if (sendSci(SCI_GETFOLDLEVEL, lineNum) & SC_FOLDLEVELHEADERFLAG)
                sendSci(SCI_TOGGLEFOLD, lineNum);
            else
                openItem(lineNum);
        }
    }

    _lock.Unlock();
}


/**
 *  \brief
 */
void ScintillaViewUI::onTabChange()
{
    if (!_lock.TryLock())
        return;

    Tab* tab = getTab();
    if (tab)
        loadTab(tab);

    _lock.Unlock();
}


/**
 *  \brief
 */
void ScintillaViewUI::onCloseTab()
{
    if (!_lock.TryLock())
        return;

    int i = TabCtrl_GetCurSel(_hTab);
    delete _activeTab;
    _activeTab = NULL;
    TabCtrl_DeleteItem(_hTab, i);

    if (TabCtrl_GetItemCount(_hTab))
    {
        i = i ? i - 1 : 0;
        Tab* tab = getTab(i);
        TabCtrl_SetCurSel(_hTab, i);
        if (tab)
            loadTab(tab);
    }
    else
    {
        sendSci(SCI_SETREADONLY, 0);
        sendSci(SCI_CLEARALL);
        sendSci(SCI_SETREADONLY, 1);

        INpp& npp = INpp::Get();
        npp.UpdateDockingWin(_hWnd);
        npp.HideDockingWin(_hWnd);
        SetFocus(npp.ReadSciHandle());
    }

    _lock.Unlock();
}


/**
 *  \brief
 */
void ScintillaViewUI::closeAllTabs()
{
    _activeTab = NULL;

    for (int i = TabCtrl_GetItemCount(_hTab); i; i--)
    {
        Tab* tab = getTab(i - 1);
        if (tab)
            delete tab;
        TabCtrl_DeleteItem(_hTab, i - 1);
    }

    sendSci(SCI_SETREADONLY, 0);
    sendSci(SCI_CLEARALL);
    sendSci(SCI_SETREADONLY, 1);

    INpp::Get().HideDockingWin(_hWnd);
}


/**
 *  \brief
 */
void ScintillaViewUI::onResize(int width, int height)
{
    RECT win = {0, 0, width, height};

    MoveWindow(_hTab, 0, 0, width, height, TRUE);
    TabCtrl_AdjustRect(_hTab, FALSE, &win);
    MoveWindow(_hSci, win.left, win.top,
            win.right - win.left, win.bottom - win.top, TRUE);
}


/**
 *  \brief
 */
LRESULT APIENTRY ScintillaViewUI::wndProc(HWND hwnd, UINT umsg,
        WPARAM wparam, LPARAM lparam)
{
    ScintillaViewUI* ui;

    switch (umsg)
    {
        case WM_CREATE:
            ui = (ScintillaViewUI*)((LPCREATESTRUCT)lparam)->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, PtrToUlong(ui));
            return 0;

        case WM_SETFOCUS:
            ui = reinterpret_cast<ScintillaViewUI*>(static_cast<LONG_PTR>
                    (GetWindowLongPtr(hwnd, GWLP_USERDATA)));
            SetFocus(ui->_hSci);
            return 0;

        case WM_NOTIFY:
            ui = reinterpret_cast<ScintillaViewUI*>(static_cast<LONG_PTR>
                    (GetWindowLongPtr(hwnd, GWLP_USERDATA)));

            switch (((LPNMHDR)lparam)->code)
            {
                case SCN_STYLENEEDED:
                    ui->onStyleNeeded((SCNotification*)lparam);
                    return 0;

                case SCN_DOUBLECLICK:
                    ui->onDoubleClick((SCNotification*)lparam);
                    return 0;

                case SCN_CHARADDED:
                    ui->onCharAddTry((SCNotification*)lparam);
                    return 0;

                case TCN_SELCHANGE:
                    ui->onTabChange();
                    return 0;
            }
        break;

        case WM_CONTEXTMENU:
            ui = reinterpret_cast<ScintillaViewUI*>(static_cast<LONG_PTR>
                    (GetWindowLongPtr(hwnd, GWLP_USERDATA)));
            ui->onCloseTab();
        break;

        case WM_SIZE:
            ui = reinterpret_cast<ScintillaViewUI*>(static_cast<LONG_PTR>
                    (GetWindowLongPtr(hwnd, GWLP_USERDATA)));
            ui->onResize(LOWORD(lparam), HIWORD(lparam));
            return 0;

        case WM_DESTROY:
            return 0;
    }

    return DefWindowProc(hwnd, umsg, wparam, lparam);
}
