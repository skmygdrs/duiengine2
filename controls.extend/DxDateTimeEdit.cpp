#include "StdAfx.h"

#include "DxContainer.h"
#include "DxDateTimeEdit.h"

/////////////////////////////////////////////////////////////////////////////
// CDxMaskEdit
/////////////////////////////////////////////////////////////////////////////
CDxMaskEdit::CDxMaskEdit()
    : m_nStartChar(0)
    , m_nEndChar(0)
    , m_bOverType(FALSE)
    , m_bUseMask(TRUE)
    , m_bModified(FALSE)
    , m_strWindowText(_T(""))
    , m_strMask(_T(""))
    , m_strLiteral(_T(""))
    , m_strDefault(_T(""))
    , m_chPrompt(_T('_'))
{
}

void CDxMaskEdit::SetUseMask(BOOL bUseMask)
{
    m_bUseMask = bUseMask;
}

BOOL CDxMaskEdit::CanUseMask() const
{
    return m_bUseMask && ((m_dwStyle & ES_READONLY) == 0) && !m_strMask.IsEmpty();
}

void CDxMaskEdit::SetOverType(BOOL bOverType)
{
    m_bOverType = bOverType;
}

BOOL CDxMaskEdit::CanOverType() const
{
    return m_bOverType;
}

BOOL CDxMaskEdit::PosInRange(int nPos) const
{
    return ((nPos >= 0) && (nPos < m_strLiteral.GetLength()));
}

TCHAR CDxMaskEdit::GetPromptChar() const
{
    return m_chPrompt;
}

CString CDxMaskEdit::GetPromptString(int nLength) const
{
    CString strPrompt;

    while (nLength--)
        strPrompt += m_chPrompt;

    return strPrompt;
}

void CDxMaskEdit::SetPromptChar(TCHAR ch, BOOL bAutoReplace)
{
    if (m_chPrompt == ch)
        return;

    if (bAutoReplace)
    {
        GetMaskState();

        for (int i = 0; i < m_strLiteral.GetLength(); i++)
            if (m_strLiteral[i] == m_chPrompt) m_strLiteral.SetAt(i, ch);

        for (int j = 0; j < m_strWindowText.GetLength(); j++)
            if (m_strWindowText[j] == m_chPrompt) m_strWindowText.SetAt(j, ch);

        SetMaskState();
    }

    m_chPrompt = ch;
}

BOOL CDxMaskEdit::MaskCut()
{
    //if (!CanUseMask())
    //    return (BOOL)DefWindowProc(WM_CUT, 0, 0);

    MaskCopy();
    MaskClear();

    return TRUE;
}

BOOL CDxMaskEdit::MaskCopy()
{
    //if (!CanUseMask())
    //    return (BOOL)DefWindowProc(WM_COPY, 0, 0);

    GetMaskState();

    CString strMaskedText = GetMaskedText(m_nStartChar, m_nEndChar);
    CopyToClipboard(strMaskedText);

    return TRUE;
}

void CDxMaskEdit::MaskReplaceSel(LPCTSTR lpszNewText)
{
    ATLASSERT(CanUseMask());

    if (m_nStartChar != m_nEndChar)
        MaskDeleteSel();

    int x = m_nStartChar, nNewTextLen = (int)_tcslen(lpszNewText);
    int nWindowTextLen = m_strWindowText.GetLength();

    if (x >= nWindowTextLen)
        return;

    for (int i = 0; i < nNewTextLen; ++i)
    {
        TCHAR ch = lpszNewText[i];

        if (ch == m_chPrompt || CheckChar(ch, x))
        {
            InsertCharAt(x, ch);
            x++;

            while (x < nWindowTextLen && !IsPromptPos(x))
                x++;

            if (x >= m_strWindowText.GetLength())
                break;
        }
    }
    CorrectPosition(x);
    m_nStartChar = m_nEndChar = x;
}

BOOL CDxMaskEdit::MaskPaste()
{
    //if (!CanUseMask())
    //    return (BOOL)DefWindowProc(WM_PASTE, 0, 0);

    GetMaskState();

    if (!OpenClipboard(NULL))
        return FALSE;

#ifndef _UNICODE
    HGLOBAL hglbPaste = ::GetClipboardData(CF_TEXT);
#else
    HGLOBAL hglbPaste = ::GetClipboardData(CF_UNICODETEXT);
#endif

    if (hglbPaste != NULL)
    {
        TCHAR* lpszClipboard = (TCHAR*)GlobalLock(hglbPaste);

        MaskReplaceSel(lpszClipboard);

        GlobalUnlock(hglbPaste);

        SetMaskState();
    }
    ::CloseClipboard();

    return TRUE;
}

void CDxMaskEdit::MaskDeleteSel()
{
    if (m_nStartChar == m_nEndChar)
        return;

    CString strMaskedText = GetMaskedText(m_nEndChar);
    SetMaskedText(strMaskedText, m_nStartChar, FALSE);

    m_nEndChar = m_nStartChar;
}

BOOL CDxMaskEdit::MaskClear()
{
    //if (!CanUseMask())
    //    return (BOOL)DefWindowProc(WM_CLEAR, 0, 0);

    GetMaskState();

    MaskDeleteSel();

    SetMaskState();

    return TRUE;
}

void CDxMaskEdit::MaskSelectAll()
{
    if (!CanUseMask())
    {
        SetSel(0, -1);
    }
    else
    {
        m_nStartChar = 0;
        CorrectPosition(m_nStartChar);
        SetSel(m_nStartChar, -1);
    }
}

BOOL CDxMaskEdit::IsModified() const
{
    return m_bModified;
}

void CDxMaskEdit::SetMaskedText(LPCTSTR lpszMaskedText, int nPos, BOOL bUpdateWindow)
{
    int nMaskedTextLength = (int)_tcslen(lpszMaskedText);

    m_strWindowText = m_strWindowText.Left(nPos);

    int nIndex = 0;
    for (; (nPos <  m_strLiteral.GetLength()) && (nIndex < nMaskedTextLength) ; nPos++)
    {
        TCHAR uChar = lpszMaskedText[nIndex];

        if (IsPromptPos(nPos) && ((uChar == m_chPrompt) || ProcessMask(uChar, nPos)))
        {
            m_strWindowText += (TCHAR)uChar;
            nIndex ++;
        }
        else
        {
            m_strWindowText += m_strLiteral[nPos];
        }
    }

    if (bUpdateWindow)
    {
        SetMaskState();
    }
    else
    {
        CorrectWindowText();
    }
}

BOOL CDxMaskEdit::SetEditMask(LPCTSTR lpszMask, LPCTSTR lpszLiteral, LPCTSTR lpszDefault)
{
    ATLASSERT(lpszMask);
    ATLASSERT(lpszLiteral);

    // initialize the mask for the control.
    m_strMask    = lpszMask;
    m_strLiteral = lpszLiteral;

    ATLASSERT(m_strMask.GetLength() == m_strLiteral.GetLength());

    if (m_strMask.GetLength() != m_strLiteral.GetLength())
        return FALSE;

    if (lpszDefault == NULL)
    {
        m_strWindowText = m_strDefault = lpszLiteral;
    }
    else
    {
        m_strWindowText = m_strDefault = lpszDefault;

        if (m_strDefault.GetLength() != m_strLiteral.GetLength())
        {
            SetMaskedText(m_strDefault, 0, FALSE);
            m_strDefault = m_strWindowText;
        }
    }

    ATLASSERT(m_strWindowText.GetLength() == m_strLiteral.GetLength());

    // set the window text for the control.
    m_bModified = FALSE;
    SetWindowText(m_strWindowText);

    return TRUE;
}

TCHAR CDxMaskEdit::ConvertUnicodeAlpha(TCHAR nChar, BOOL bUpperCase) const
{
    CString strTemp(nChar);
    if (bUpperCase)
        strTemp.MakeUpper();
    else
        strTemp.MakeLower();

    return strTemp[0];
}

BOOL CDxMaskEdit::CheckChar(TCHAR& nChar, int nPos)
{
    // do not use mask
    if (!CanUseMask())
        return FALSE;

    // control character, OK
    if (!IsPrintChar(nChar))
        return TRUE;

    // make sure the string is not longer than the mask
    if (nPos >= m_strMask.GetLength())
        return FALSE;

    if (!IsPromptPos(nPos))
        return FALSE;

    return ProcessMask(nChar, nPos);
}

BOOL CDxMaskEdit::ProcessMask(TCHAR& nChar, int nEndPos)
{
    ATLASSERT(nEndPos < m_strMask.GetLength());
    if (nEndPos < 0 || nEndPos >= m_strMask.GetLength())
        return FALSE;

    // check the key against the mask
    switch (m_strMask.GetAt(nEndPos))
    {
    case '0':       // digit only //completely changed this
        return _istdigit(nChar);

    case '9':       // digit or space
        return _istdigit(nChar) || _istspace(nChar);

    case '#':       // digit or space or '+' or '-'
        return _istdigit(nChar) || (_istspace(nChar) || nChar == _T('-') || nChar == _T('+'));

    case 'd':       // decimal
        return _istdigit(nChar) || (_istspace(nChar) || nChar == _T('-') || nChar == _T('+') || nChar == _T('.') || nChar == _T(','));

    case 'L':       // alpha only
        return IsAlphaChar(nChar);

    case '?':       // alpha or space
        return IsAlphaChar(nChar) || _istspace(nChar);

    case 'A':       // alpha numeric only
        return _istalnum(nChar) || IsAlphaChar(nChar);

    case 'a':       // alpha numeric or space
        return _istalnum(nChar) || IsAlphaChar(nChar) || _istspace(nChar);

    case '&':       // all print character only
        return IsPrintChar(nChar);

    case 'H':       // hex digit
        return _istxdigit(nChar);

    case 'X':       // hex digit or space
        return _istxdigit(nChar) || _istspace(nChar);

    case '>':
        if (IsAlphaChar(nChar))
        {
            nChar = ConvertUnicodeAlpha(nChar, TRUE);
            return TRUE;
        }
        return FALSE;

    case '<':
        if (IsAlphaChar(nChar))
        {
            nChar = ConvertUnicodeAlpha(nChar, FALSE);
            return TRUE;
        }
        return FALSE;
    }

    return FALSE;
}

#if 0
BOOL CDxMaskEdit::PreTranslateMessage(MSG* pMsg)
{
    if (!CanUseMask())
        return TBase::PreTranslateMessage(pMsg);

    // intercept Ctrl+C (copy), Ctrl+V (paste), Ctrl+X (cut) and Ctrl+Z (undo)
    // before CEdit base class gets a hold of them.

    if (pMsg->message == WM_KEYDOWN)
    {
        if (::GetKeyState(VK_SUBTRACT) < 0)
        {
            OnChar('-', 1, 1);
            return TRUE;
        }

        if (::GetKeyState(VK_ADD) < 0)
        {
            OnChar('+', 1, 1);
            return TRUE;
        }

        if (::GetKeyState(VK_CONTROL) < 0)
        {
            switch (pMsg->wParam)
            {
            case 'X':
            case 'x':
                {
                    MaskCut();
                    return TRUE;
                }

            case 'C':
            case 'c':
                {
                    MaskCopy();
                    return TRUE;
                }

            case 'V':
            case 'v':
                {
                    MaskPaste();
                    return TRUE;
                }

            case 'Z':
            case 'z':
                {
                    MaskUndo();
                    return TRUE;
                }
            }
        }
    }

    return TBase::PreTranslateMessage(pMsg);
}
#endif

void CDxMaskEdit::DeleteCharAt(int nPos)
{
    ATLASSERT(PosInRange(nPos));

    if (!PosInRange(nPos))
        return;

    CString strMaskedText = GetMaskedText(nPos + 1) + m_chPrompt;

    SetMaskedText(strMaskedText, nPos, FALSE);
}

void CDxMaskEdit::InsertCharAt(int nPos, TCHAR nChar)
{
    ATLASSERT(PosInRange(nPos));

    if (!PosInRange(nPos))
        return;

    CString strMaskedText = CString(nChar) + GetMaskedText(nPos);

    SetMaskedText(strMaskedText, nPos, FALSE);
}

BOOL CDxMaskEdit::CopyToClipboard(const CString& strText)
{
    if (!OpenClipboard(NULL))
        return FALSE;

    ::EmptyClipboard();

    int nLen = (strText.GetLength() + 1) * sizeof(TCHAR);

    HGLOBAL hglbCopy = ::GlobalAlloc(GMEM_MOVEABLE, nLen);

    if (hglbCopy == NULL)
    {
        ::CloseClipboard();
        return FALSE;
    }

    LPTSTR lptstrCopy = (TCHAR*)GlobalLock(hglbCopy);
    _tcscpy_s(lptstrCopy, strText.GetLength() + 1, (LPCTSTR)strText);
    GlobalUnlock(hglbCopy);

#ifndef _UNICODE
    ::SetClipboardData(CF_TEXT, hglbCopy);
#else
    ::SetClipboardData(CF_UNICODETEXT, hglbCopy);
#endif

    if (!::CloseClipboard())
        return FALSE;

    return TRUE;
}

CString CDxMaskEdit::GetMaskedText(int nStartPos, int nEndPos) const
{
    if (nEndPos == -1)
        nEndPos = m_strWindowText.GetLength();
    else
        nEndPos = min(nEndPos, m_strWindowText.GetLength());

    CString strBuffer;

    for (int i = nStartPos; i < nEndPos; ++i)
    {
        if (IsPromptPos(i))
        {
            strBuffer += m_strWindowText[i];
        }
    }

    return strBuffer;
}

void CDxMaskEdit::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if (!CanUseMask())
    {
        __super::OnKeyDown(nChar, nRepCnt, nFlags);
        return;
    }

    BOOL bShift = (::GetKeyState(VK_SHIFT) < 0);
    BOOL bCtrl  = (::GetKeyState(VK_CONTROL) < 0);

    switch (nChar)
    {
    case VK_UP:
    case VK_LEFT:
    case VK_HOME:
        {
            __super::OnKeyDown(nChar, nRepCnt, nFlags);

            GetMaskState(FALSE);

            int nStartChar = m_nStartChar;
            CorrectPosition(nStartChar, FALSE);

            if (m_nStartChar < nStartChar)
            {
                m_nStartChar = nStartChar;

                if (!bShift)
                    m_nEndChar = nStartChar;
            }

            SetMaskState();
        }
        return;

    case VK_DOWN:
    case VK_RIGHT:
    case VK_END:
        {
            __super::OnKeyDown(nChar, nRepCnt, nFlags);

            GetMaskState(FALSE);

            int iEndChar = m_nEndChar;
            CorrectPosition(iEndChar);

            if (m_nEndChar > iEndChar)
            {
                m_nEndChar = iEndChar;

                if (!bShift)
                    m_nStartChar = iEndChar;
            }

            SetMaskState();
        }
        return;

    case VK_INSERT:
        {
            if (bCtrl)
            {
                MaskCopy();
            }
            else if (bShift)
            {
                MaskPaste();
            }
            else
            {
                m_bOverType = !m_bOverType; // set the type-over flag
            }
        }
        return;

    case VK_DELETE:
        {
            GetMaskState();

            if (m_nStartChar == m_nEndChar)
            {
                m_nEndChar = m_nStartChar +1;
            }
            else if (bShift)
            {
                MaskCopy();
            }

            MaskDeleteSel();
            SetMaskState();
        }
        return;

    case VK_SPACE:
        {
            GetMaskState();

            if (!PosInRange(m_nStartChar) || !IsPromptPos(m_nStartChar))
            {
                NotifyPosNotInRange();
                return;
            }

            TCHAR chSpace = _T(' ');

            if (!ProcessMask(chSpace, m_nStartChar))
                chSpace = m_chPrompt;

            ProcessChar(chSpace);

            SetMaskState();
        }
        return;

    case VK_BACK:
        {
            GetMaskState(FALSE);

            if (((m_nStartChar > 0) || (m_nStartChar == 0 && m_nEndChar != 0))  && (m_nStartChar <= m_strLiteral.GetLength()))
            {
                if (m_nStartChar == m_nEndChar)
                {
                    m_nStartChar--;
                    CorrectPosition(m_nStartChar, FALSE);

                    if (m_bOverType && PosInRange(m_nStartChar))
                    {
                        m_strWindowText.SetAt(m_nStartChar, m_strDefault[m_nStartChar]);
                        m_nEndChar = m_nStartChar;
                    }
                }

                MaskDeleteSel();
                SetMaskState();
            }
            else
            {
                NotifyPosNotInRange();
            }
        }
        return;
    }

    __super::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CDxMaskEdit::ProcessChar(TCHAR nChar)
{
    int nLen = m_strLiteral.GetLength();

    if (m_nStartChar >= nLen)
    {
        NotifyPosNotInRange();
        return;
    }

    if (m_nStartChar != m_nEndChar)
    {
        MaskDeleteSel();
    }

    ATLASSERT(m_nStartChar == m_nEndChar);

    CorrectPosition(m_nStartChar);

    if (CanOverType())
    {
        m_strWindowText.SetAt(m_nStartChar, nChar);
    }
    else
    {
        InsertCharAt(m_nStartChar, nChar);
    }

    if (m_nStartChar < nLen)
        m_nStartChar++;

    CorrectPosition(m_nStartChar);

    m_nEndChar = m_nStartChar;
}

int CDxMaskEdit::OnCreate(LPVOID)
{
    int nRet = __super::OnCreate(NULL);
    if (nRet != 0)
        return nRet;

    return 0;
}

void CDxMaskEdit::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if (!CanUseMask())
    {
        __super::OnChar(nChar, nRepCnt, nFlags);
        return;
    }

    switch (nChar)
    {
    case VK_SPACE:
    case VK_BACK:
        return; // handled in WM_KEYDOWN
    }

    GetMaskState();

    if (!PosInRange(m_nStartChar) || !IsPromptPos(m_nStartChar))
    {
        NotifyPosNotInRange();
        return;
    }

    TCHAR ch = (TCHAR)nChar;

    if (!CheckChar(ch, m_nStartChar))
    {
        NotifyInvalidCharacter(ch, m_strMask[m_nStartChar]);
        return;
    }

    if (IsPrintChar(ch))
    {
        ProcessChar(ch);
        SetMaskState();
    }
    else
    {
        if (nChar != 127)
            __super::OnChar(nChar, nRepCnt, nFlags);
    }
}

void CDxMaskEdit::OnSetDxFocus()
{
    __super::OnSetDxFocus();

    if (!CanUseMask())
    {
        return;
    }

    MaskGetSel();
    CorrectPosition(m_nStartChar);

    m_nEndChar = m_nStartChar;
    SetSel(m_nStartChar, m_nEndChar);
}

// Some goodies
BOOL CDxMaskEdit::CorrectPosition(int& nPos, BOOL bForward)
{
    int nLen = m_strLiteral.GetLength();

    if (IsPromptPos(nPos))
        return TRUE;

    if (bForward)
    {
        for (; nPos < nLen; nPos++)
        {
            if (IsPromptPos(nPos))
                return TRUE;
        }

        for (; nPos >= 0; nPos--)
        {
            if (IsPromptPos(nPos - 1))
                return FALSE;
        }
    }
    else
    {
        for (; nPos >= 0; nPos--)
        {
            if (IsPromptPos(nPos))
                return TRUE;
        }

        for (; nPos < nLen; nPos++)
        {
            if (IsPromptPos(nPos))
                return FALSE;
        }
    }

    return FALSE;
}

BOOL CDxMaskEdit::IsPrintChar(TCHAR nChar)
{
    return _istprint(nChar) || IsAlphaChar(nChar);
}

BOOL CDxMaskEdit::IsAlphaChar(TCHAR nChar)
{
    if (_istalpha(nChar))
        return TRUE;

    if (ConvertUnicodeAlpha(nChar, TRUE) != nChar)
        return TRUE;

    if (ConvertUnicodeAlpha(nChar, FALSE) != nChar)
        return TRUE;

    return FALSE;
}

void CDxMaskEdit::NotifyPosNotInRange()
{
    ::MessageBeep((UINT)-1);
}

void CDxMaskEdit::NotifyInvalidCharacter(TCHAR /*nChar*/, TCHAR /*chMask*/)
{
    ::MessageBeep((UINT)-1);
}

BOOL CDxMaskEdit::IsPromptPos(int nPos) const
{
    return IsPromptPos(m_strLiteral, nPos);
}

BOOL CDxMaskEdit::IsPromptPos(const CString& strLiteral, int nPos) const
{
    return (nPos >= 0 && nPos < strLiteral.GetLength()) && (strLiteral[nPos] == m_chPrompt);
}

void CDxMaskEdit::CorrectWindowText()
{
    int nLiteralLength = m_strLiteral.GetLength();
    int nWindowTextLength = m_strWindowText.GetLength();

    if (nWindowTextLength > nLiteralLength)
    {
        m_strWindowText = m_strWindowText.Left(nLiteralLength);
    }
    else if (nWindowTextLength < nLiteralLength)
    {
        m_strWindowText += m_strLiteral.Mid(nWindowTextLength, nLiteralLength - nWindowTextLength);
    }
}

void CDxMaskEdit::GetMaskState(BOOL bCorrectSelection)
{
    //if (!m_hWnd)
    //    return;

    ATLASSERT(m_bUseMask);

    MaskGetSel();
    m_strWindowText = GetWindowText();

    ATLASSERT(m_strDefault.GetLength() == m_strLiteral.GetLength());
    ATLASSERT(m_strMask.GetLength() == m_strLiteral.GetLength());

    CorrectWindowText();

    if (bCorrectSelection)
    {
        CorrectPosition(m_nStartChar);
        CorrectPosition(m_nEndChar);

        if (m_nEndChar < m_nStartChar)
            m_nEndChar = m_nStartChar;
    }
}

void CDxMaskEdit::MaskGetSel()
{
    GetSel(m_nStartChar, m_nEndChar);
}

void CDxMaskEdit::SetMaskState()
{
    //if (!m_hWnd)
    //    return;

    ATLASSERT(m_bUseMask);

    CString strWindowText = GetWindowText();

    CorrectWindowText();
    GetContainer()->DxShowCaret(FALSE);

    if (strWindowText != m_strWindowText)
    {
        SetWindowText(m_strWindowText);

        m_bModified = TRUE;
    }

    SetSel(MAKELONG(m_nStartChar, m_nEndChar), FALSE);
    GetContainer()->DxShowCaret(TRUE);
}

/////////////////////////////////////////////////////////////////////////////
// CDxDateEdit class
/////////////////////////////////////////////////////////////////////////////
CDxDateEdit::CDxDateEdit()
{
    m_bUseMask   = true;
    m_strMask    = _T("0000/00/00");
    m_strLiteral = _T("____/__/__");
    m_strDefault = _T("0000/00/00");
}

int CDxDateEdit::OnCreate(LPVOID)
{
    int nRet = __super::OnCreate(NULL);
    if (nRet != 0)
        return nRet;

    SetDateTime(COleDateTime::GetCurrentTime());

    return 0;
}

COleDateTime CDxDateEdit::ReadOleDateTime(LPCTSTR lpszData)
{
    COleDateTime dt;
    dt.ParseDateTime(lpszData);
    return dt;
}

void CDxDateEdit::FormatOleDateTime(CString &strData, COleDateTime dt)
{
    strData = dt.Format(_T("%Y/%m/%d"));
}

void CDxDateEdit::SetDateTime(COleDateTime& dt)
{
    CString strText;
    FormatOleDateTime(strText, dt);
    m_strWindowText = m_strDefault = strText;
    SetWindowText(strText);
}

void CDxDateEdit::SetDateTime(LPCTSTR strDate)
{
    m_strWindowText = m_strDefault = strDate;
    SetWindowText(strDate);
}

COleDateTime CDxDateEdit::GetDateTime()
{
    CString strText = GetWindowText();
    return ReadOleDateTime(strText);
}

CString CDxDateEdit::GetWindowDateTime()
{
    CString strText = GetWindowText();
    return strText;
}

BOOL CDxDateEdit::ProcessMask(TCHAR& nChar, int nEndPos)
{
    // check the key against the mask
    if (m_strMask[nEndPos] == _T('0') && _istdigit((TCHAR)nChar))
    {
        // Allow d/m/y and m/d/y

        if (nEndPos == 0 || nEndPos == 3)
        {
            if (nChar > '3')
                return FALSE;
        }
        if (nEndPos == 1 || nEndPos == 4)
        {
            if (m_strWindowText.GetAt(0) == _T('3'))
            {
                if (nChar > _T('1'))
                    return FALSE;
            }
        }

        return TRUE;
    }

    return FALSE;
}

/////////////////////////////////////////////////////////////////////////////
// CDxTimeEdit class
/////////////////////////////////////////////////////////////////////////////

CDxTimeEdit::CDxTimeEdit()
{
    m_bMilitary  = false;
    m_bUseMask   = true;
    m_strMask    = _T("00:00");
    m_strLiteral = _T("__:__");
    m_strDefault = _T("00:00");
    m_nHours     = 0;
    m_nMins      = 0;
}

int CDxTimeEdit::OnCreate(LPVOID)
{
    int nRet = __super::OnCreate(NULL);
    if (nRet != 0)
        return nRet;

    SetDateTime(COleDateTime::GetCurrentTime());

    return 0;
}

void CDxTimeEdit::FormatOleDateTime(CString &strData, COleDateTime dt)
{
    if (dt.m_dt == 0)
    {
        strData = _T("00:00");
    }
    else
    {
        strData = dt.Format(_T("%H:%M"));
    }
}

BOOL CDxTimeEdit::ProcessMask(TCHAR& nChar, int nEndPos)
{
    // check the key against the mask
    if (m_strMask[nEndPos] == _T('0') && _istdigit(nChar))
    {
        switch (nEndPos)
        {
        case 0:
            if (m_bMilitary)
            {
                if (nChar > _T('2'))
                    return FALSE;
            }
            else
            {
                if (nChar > _T('1'))
                    return FALSE;
            }
            return TRUE;

        case 1:
            if (m_bMilitary)
            {
                if (m_strWindowText.GetAt(0) == _T('2'))
                {
                    if (nChar > _T('3'))
                        return FALSE;
                }
            }
            else
            {
                if (m_strWindowText.GetAt(0) == _T('1'))
                {
                    if (nChar > _T('2'))
                        return FALSE;
                }
            }
            return TRUE;

        case 3:
            if (nChar > _T('5'))
                return FALSE;
            return TRUE;

        case 4:
            return TRUE;
        }
    }

    return FALSE;
}

void CDxTimeEdit::SetHours(int nHours)
{
    m_nHours = nHours;

    CString strText;
    strText.Format(_T("%02d:%02d"), m_nHours, m_nMins);
    SetWindowText(strText);
}

void CDxTimeEdit::SetMins(int nMins)
{
    m_nMins = nMins;

    CString strText;
    strText.Format(_T("%02d:%02d"), m_nHours, m_nMins);
    SetWindowText(strText);
}

void CDxTimeEdit::SetTime(int nHours, int nMins)
{
    m_nHours = nHours;
    m_nMins = nMins;

    CString strText;
    strText.Format(_T("%02d:%02d"), m_nHours, m_nMins);
    SetWindowText(strText);
}

