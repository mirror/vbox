'
' Copyright (C) 2009 Sun Microsystems, Inc.
'
' This file is part of VirtualBox Open Source Edition (OSE), as
' available from http://www.virtualbox.org. This file is free software;
' you can redistribute it and/or modify it under the terms of the GNU
' General Public License (GPL) as published by the Free Software
' Foundation, in version 2 as it comes in the "COPYING" file of the
' VirtualBox OSE distribution. VirtualBox OSE is distributed in the
' hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
'
' Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
' Clara, CA 95054 USA or visit http://www.sun.com if you need
' additional information or have any questions.
'
Imports System
Imports System.Drawing
Imports System.Windows.Forms

Module Module1

    Sub Main()
        Dim vb As VirtualBox.IVirtualBox
        Dim listBox As New ListBox()
        Dim form As New Form
        
        vb = CreateObject("VirtualBox.VirtualBox")
        
        form.Text = "VirtualBox version " & vb.Version
        form.Size = New System.Drawing.Size(400, 320)
        form.Location = New System.Drawing.Point(10, 10)

        listBox.Size = New System.Drawing.Size(350, 200)
        listBox.Location = New System.Drawing.Point(10, 10)

        For Each m In vb.Machines
            listBox.Items.Add(m.Name & " [" & m.Id & "]")
        Next

        form.Controls.Add(listBox)

        'form.ShowDialog()
        form.Show()
        MsgBox("OK")
    End Sub

End Module
