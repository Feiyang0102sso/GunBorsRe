# Close only the SDL game window owned by the test process, including hidden windows.
Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;

public static class ZRuntimeTestWindow {
    private delegate bool WindowVisitor(IntPtr window, IntPtr state);
    [DllImport("user32.dll")]
    private static extern bool EnumWindows(WindowVisitor visitor, IntPtr state);
    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetWindowText(IntPtr window, StringBuilder text, int length);
    [DllImport("user32.dll")]
    private static extern bool PostMessage(IntPtr window, uint message, IntPtr key, IntPtr data);

    public static bool Close(uint processId) {
        bool closed = false;
        EnumWindows((window, state) => {
            uint owner;
            GetWindowThreadProcessId(window, out owner);
            if (owner != processId) { return true; }
            var title = new StringBuilder(256);
            GetWindowText(window, title, title.Capacity);
            if (title.ToString() != "Gun Bros") { return true; }
            closed = PostMessage(window, 0x0010, IntPtr.Zero, IntPtr.Zero);
            return !closed;
        }, IntPtr.Zero);
        return closed;
    }
}
'@
