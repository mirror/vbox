<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.0" language="en">
<context>
    <name>UIMessageCenter</name>
    <message numerus="yes">
        <source>&lt;p&gt;The %n following virtual machine(s) are currently in a saved state: &lt;b&gt;%1&lt;/b&gt;&lt;/p&gt;&lt;p&gt;If you continue the runtime state of the exported machine(s) will be discarded. The other machine(s) will not be changed.&lt;/p&gt;</source>
        <translation type="obsolete">
            <numerusform>&lt;p&gt;The virtual machine &lt;b&gt;%1&lt;/b&gt; is currently in a saved state.&lt;/p&gt;&lt;p&gt;If you continue the runtime state of the exported machine will be discarded. Note that the existing machine is not changed.&lt;/p&gt;</numerusform>
            <numerusform>&lt;p&gt;The virtual machines &lt;b&gt;%1&lt;/b&gt; are currently in a saved state.&lt;/p&gt;&lt;p&gt;If you continue the runtime state of the exported machines will be discarded. Note that the existing machines are not changed.&lt;/p&gt;</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <location filename="../src/globals/UIMessageCenter.cpp" line="2440"/>
        <source>&lt;p&gt;The %n following virtual machine(s) are currently in a saved state: &lt;b&gt;%1&lt;/b&gt;&lt;/p&gt;&lt;p&gt;If you continue the runtime state of the exported machine(s) will be discarded. The other machine(s) will not be changed.&lt;/p&gt;</source>
        <comment>This text is never used with n == 0.  Feel free to drop the %n where possible, we only included it because of problems with Qt Linguist (but the user can see how many machines are in the list and doesn&apos;t need to be told).</comment>
        <translation type="unfinished">
            <numerusform>&lt;p&gt;The virtual machine &lt;b&gt;%1&lt;/b&gt; is currently in a saved state.&lt;/p&gt;&lt;p&gt;If you continue the runtime state of the exported machine will be discarded. Note that the existing machine is not changed.&lt;/p&gt;</numerusform>
            <numerusform>&lt;p&gt;The virtual machines &lt;b&gt;%1&lt;/b&gt; are currently in a saved state.&lt;/p&gt;&lt;p&gt;If you continue the runtime state of the exported machines will be discarded. Note that the existing machines are not changed.&lt;/p&gt;</numerusform>
        </translation>
    </message>
</context>
<context>
    <name>VBoxGlobal</name>
    <message numerus="yes">
        <location filename="../src/globals/VBoxGlobal.h" line="163"/>
        <source>%n year(s)</source>
        <translation>
            <numerusform>%n year</numerusform>
            <numerusform>%n years</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <location filename="../src/globals/VBoxGlobal.h" line="168"/>
        <source>%n month(s)</source>
        <translation>
            <numerusform>%n month</numerusform>
            <numerusform>%n months</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <location filename="../src/globals/VBoxGlobal.h" line="173"/>
        <source>%n day(s)</source>
        <translation>
            <numerusform>%n day</numerusform>
            <numerusform>%n days</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <location filename="../src/globals/VBoxGlobal.h" line="178"/>
        <source>%n hour(s)</source>
        <translation>
            <numerusform>%n hour</numerusform>
            <numerusform>%n hours</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <location filename="../src/globals/VBoxGlobal.h" line="188"/>
        <source>%n second(s)</source>
        <translation>
            <numerusform>%n second</numerusform>
            <numerusform>%n seconds</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <location filename="../src/globals/VBoxGlobal.h" line="183"/>
        <source>%n minute(s)</source>
        <translation>
            <numerusform>%n minute</numerusform>
            <numerusform>%n minutes</numerusform>
        </translation>
    </message>
</context>
<context>
    <name>VBoxTakeSnapshotDlg</name>
    <message numerus="yes">
        <location filename="../src/VBoxTakeSnapshotDlg.cpp" line="81"/>
        <source>Warning: You are taking a snapshot of a running machine which has %n immutable image(s) attached to it. As long as you are working from this snapshot the immutable image(s) will not be reset to avoid loss of data.</source>
        <translation>
            <numerusform>Warning: You are taking a snapshot of a running machine which has %n immutable image attached to it. As long as you are working from this snapshot the immutable image will not be reset to avoid loss of data.</numerusform>
            <numerusform>Warning: You are taking a snapshot of a running machine which has %n immutable images attached to it. As long as you are working from this snapshot the immutable images will not be reset to avoid loss of data.</numerusform>
        </translation>
    </message>
</context>
</TS>
