"""ZiiNAN: Existing UI/text/input/chat runtime; local-only fixture, no outgoing chat/network actions."""
import chat
import ui
import wndMgr
from ui_fixture_panels import UiPanels


class LocalChat(ui.Window):
    def __init__(self, width, height):
        ui.Window.__init__(self, "TOP_MOST")
        self.chat_id = chat.CreateChatSet(chat.CHAT_SET_CHAT_WINDOW)
        chat.SetBoardState(self.chat_id, chat.BOARD_STATE_EDIT)
        chat.SetPosition(self.chat_id, 30, height - 155)
        chat.SetHeight(self.chat_id, 100)
        chat.SetChatColor(chat.CHAT_TYPE_INFO, 255, 210, 100)
        for value in ("M10A local chat: no server message", "Umlaute: ÄÖÜ äöü ß é €",
                      "|cff40ff80Color tag|r and normal system text"):
            chat.AppendChat(chat.CHAT_TYPE_INFO, value)
        self.SetPosition(20, height - 275)
        self.SetSize(min(width - 40, 520), 125)
        self.EnableScissorRect()
        self.Show()

    def OnUpdate(self):
        chat.Update(self.chat_id)

    def OnRender(self):
        chat.Render(self.chat_id)

    def Destroy(self):
        chat.Clear()
        ui.Window.Destroy(self)


class TextPanels(UiPanels):
    def clear(self):
        for widget in self.extras:
            if isinstance(widget, ui.EditLine) and widget.IsFocus():
                widget.KillFocus()
        self.text_clip = None
        super().clear()

    def update(self, elapsed):
        old_phase = self.phase
        super().update(elapsed)
        if self.phase != old_phase:
            label = ui.TextLine()
            label.SetPosition(25, 10)
            label.SetText("M10A: " + self.PHASES[self.phase] + " – bestehende UI / Schrift")
            label.SetOutline()
            label.Show()
            self.extras.append(label)
            board = ui.ThinBoard("TOP_MOST")
            board.SetPosition(20, self.height - 125)
            board.SetSize(470, 66)
            board.Show()
            edit = ui.EditLine()
            edit.SetParent(board)
            edit.SetPosition(10, 8)
            edit.SetSize(430, 22)
            edit.SetMax(120)
            edit.SetLimitWidth(420)
            edit.EnableScissorRect()
            edit.SetText("Editable: ÄÖÜ äöü ß é € – phase %d" % self.phase)
            edit.Show()
            edit.SetFocus()
            edit.SetEndPosition()
            selection = ui.TextLine()
            selection.SetParent(board)
            selection.SetPosition(10, 34)
            selection.SetText("Existing selection + colored |cff50ff80text tag|r")
            wndMgr.SetSelection(selection.hWnd, 3, 12)
            selection.Show()
            self.extras.extend((board, edit, selection))
            if self.world:
                self.tooltip.AppendTextLine("Original tooltip / M10A")
                self.tooltip.AppendTextLine("ÄÖÜ äöü ß é €", 0xffe0bb70)
                self.tooltip.AppendTextLine("|cff40ff80Colored|r outline text")
                self.tooltip.SetToolTipPosition(self.width - 170, 160)
                self.tooltip.ShowToolTip()
                local_chat = LocalChat(self.width, self.height)
                self.extras.append(local_chat)
                clipped = ui.TextLine()
                clipped.SetParent(self.clip_parent)
                clipped.SetPosition(-25, 15)
                clipped.SetText("Clipped text 0123456789 ÄÖÜ repeated across the boundary")
                clipped.SetOutline()
                clipped.Show()
                self.extras.append(clipped)
                self.text_clip = clipped
            self.record("text phase=%d: native labels edit selection tooltip chat" % self.phase)
        if getattr(self, "text_clip", None):
            self.text_clip.SetPosition(int(-25 - 180 * self.scroll.GetPos()), 15)
