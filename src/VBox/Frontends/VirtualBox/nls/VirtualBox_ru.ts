<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.0" language="ru">
<context>
    <name>@@@</name>
    <message>
        <source>English</source>
        <comment>Native language name</comment>
        <translation>Русский</translation>
    </message>
    <message>
        <source>--</source>
        <comment>Native language country name (empty if this language is for all countries)</comment>
        <translation></translation>
    </message>
    <message>
        <source>English</source>
        <comment>Language name, in English</comment>
        <translation>Russian</translation>
    </message>
    <message>
        <source>--</source>
        <comment>Language country name, in English (empty if native country name is empty)</comment>
        <translation></translation>
    </message>
    <message>
        <source>Oracle Corporation</source>
        <comment>Comma-separated list of translators</comment>
        <translation>Oracle Corporation, Igor Gorbounov, Egor Morozov</translation>
    </message>
</context>
<context>
    <name>AttachmentsModel</name>
    <message>
        <source>Double-click to add a new attachment</source>
        <translation type="obsolete">Дважды щелкните мышью для создания нового подключения</translation>
    </message>
    <message>
        <source>Hard Disk</source>
        <translation type="obsolete">Жесткий диск</translation>
    </message>
    <message>
        <source>Slot</source>
        <translation type="obsolete">Разъем</translation>
    </message>
</context>
<context>
    <name>QApplication</name>
    <message>
        <source>Executable &lt;b&gt;%1&lt;/b&gt; requires Qt %2.x, found Qt %3.</source>
        <translation>Приложение &lt;b&gt;%1&lt;/b&gt; требует Qt %2.x, найден Qt %3.</translation>
    </message>
    <message>
        <source>Incompatible Qt Library Error</source>
        <translation>Ошибка - несовместимая библиотека Qt</translation>
    </message>
    <message>
        <source>VirtualBox - Error In %1</source>
        <translation>VirtualBox - Ошибка в %1</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;b&gt;%1 (rc=%2)&lt;/b&gt;&lt;br/&gt;&lt;br/&gt;</source>
        <translation>&lt;html&gt;&lt;b&gt;%1 (rc=%2)&lt;/b&gt;&lt;br/&gt;&lt;br/&gt;</translation>
    </message>
    <message>
        <source>Please try reinstalling VirtualBox.</source>
        <translation>Попробуйте выполнить повторную установку приложения VirtualBox.</translation>
    </message>
    <message>
        <source>This error means that the kernel driver was either not able to allocate enough memory or that some mapping operation failed.&lt;br/&gt;&lt;br/&gt;There are known problems with Linux 2.6.29. If you are running such a kernel, please edit /usr/src/vboxdrv-*/Makefile and enable &lt;i&gt;VBOX_USE_INSERT_PAGE = 1&lt;/i&gt;. After that, re-compile the kernel module by executing&lt;br/&gt;&lt;br/&gt;  &lt;font color=blue&gt;&apos;/etc/init.d/vboxdrv setup&apos;&lt;/font&gt;&lt;br/&gt;&lt;br/&gt;as root.</source>
        <translation type="obsolete">Данная ошибка означает, что либо драйвер ядра не смог выделить достаточное количество памяти, либо операция выделения памяти завершилась неудачно.&lt;br/&gt;&lt;br/&gt;Подобные проблемы присутствуют в ядре Linux 2.6.29. Если Вы используете ядро данной версии, пожалуйста отредактируйте /usr/src/vboxdrv-*/Makefile, добавив &lt;i&gt;VBOX_USE_INSERT_PAGE = 1&lt;/i&gt;. После этого пересоберите ядро запуском&lt;br/&gt;&lt;br/&gt;  &lt;font color=blue&gt;&apos;/etc/init.d/vboxdrv setup&apos;&lt;/font&gt;&lt;br/&gt;&lt;br/&gt;от имени администратора.</translation>
    </message>
    <message>
        <source>The VirtualBox Linux kernel driver (vboxdrv) is either not loaded or there is a permission problem with /dev/vboxdrv. Please reinstall the kernel module by executing&lt;br/&gt;&lt;br/&gt;  &lt;font color=blue&gt;&apos;/etc/init.d/vboxdrv setup&apos;&lt;/font&gt;&lt;br/&gt;&lt;br/&gt;as root. Users of Ubuntu, Fedora or Mandriva should install the DKMS package first. This package keeps track of Linux kernel changes and recompiles the vboxdrv kernel module if necessary.</source>
        <translation>Драйвер ядра VirtualBox ОС Linux (vboxdrv) вероятно не загружен, либо присутствуют проблемы с правами доступа к /dev/vboxdrv. Переконфигурируйте модуль ядра запуском&lt;br/&gt;&lt;br/&gt;  &lt;font color=blue&gt;&apos;/etc/init.d/vboxdrv setup&apos;&lt;/font&gt;&lt;br/&gt;&lt;br/&gt;от имени администратора. Пользователям Ubuntu, Fedora или Mandriva следует сперва установить пакет DKMS. Этот пакет отслеживает изменения ядра Linux и пересобирает модуль ядра vboxdrv в случае необходимости.</translation>
    </message>
    <message>
        <source>Make sure the kernel module has been loaded successfully.</source>
        <translation>Удостоверьтесь, что модуль ядра успешно загружен.</translation>
    </message>
    <message>
        <source>VirtualBox - Runtime Error</source>
        <translation>VirtualBox - Ошибка программы</translation>
    </message>
    <message>
        <source>&lt;b&gt;Cannot access the kernel driver!&lt;/b&gt;&lt;br/&gt;&lt;br/&gt;</source>
        <translation>&lt;b&gt;Нет доступа к драйверу ядра!&lt;/b&gt;&lt;br/&gt;&lt;br/&gt;</translation>
    </message>
    <message>
        <source>Unknown error %2 during initialization of the Runtime</source>
        <translation>Неизвестная ошибка инициализации программы (%2)</translation>
    </message>
    <message>
        <source>Kernel driver not accessible</source>
        <translation>Драйвер ядра не доступен</translation>
    </message>
    <message>
        <source>The VirtualBox kernel modules do not match this version of VirtualBox. The installation of VirtualBox was apparently not successful. Please try completely uninstalling and reinstalling VirtualBox.</source>
        <translation>Модуль ядра VirtualBox не совместим с текущей версией приложения. Возможно установка VirtualBox не была завершена или прошла некорректно. Попробуйте полностью удалить VirtualBox и установить заново.</translation>
    </message>
    <message>
        <source>The VirtualBox kernel modules do not match this version of VirtualBox. The installation of VirtualBox was apparently not successful. Executing&lt;br/&gt;&lt;br/&gt;  &lt;font color=blue&gt;&apos;/etc/init.d/vboxdrv setup&apos;&lt;/font&gt;&lt;br/&gt;&lt;br/&gt;may correct this. Make sure that you do not mix the OSE version and the PUEL version of VirtualBox.</source>
        <translation>Модуль ядра VirtualBox не совместим с текущей версией приложения. Возможно установка VirtualBox не была завершена или прошла некорректно. Запуск&lt;br/&gt;&lt;br/&gt;  &lt;font color=blue&gt;&apos;/etc/init.d/vboxdrv setup&apos;&lt;/font&gt;&lt;br/&gt;&lt;br/&gt;должен исправить данную проблему. Убедитесь в том, что не используете платную (PUEL) и бесплатную (OSE) версии VirtualBox одновременно.</translation>
    </message>
    <message>
        <source>This error means that the kernel driver was either not able to allocate enough memory or that some mapping operation failed.</source>
        <translation>Данная ошибка означает, что либо драйвер ядра не смог выделить достаточное количество памяти, либо некая операция с памятью неудачно завершена.</translation>
    </message>
</context>
<context>
    <name>QIArrowSplitter</name>
    <message>
        <source>&amp;Back</source>
        <translation>&amp;Назад</translation>
    </message>
    <message>
        <source>&amp;Next</source>
        <translation>Да&amp;лее</translation>
    </message>
</context>
<context>
    <name>QIFileDialog</name>
    <message>
        <source>Select a directory</source>
        <translation>Выберите каталог</translation>
    </message>
    <message>
        <source>Select a file</source>
        <translation>Выберите файл</translation>
    </message>
</context>
<context>
    <name>QIHelpButton</name>
    <message>
        <source>&amp;Help</source>
        <translation type="obsolete">Справк&amp;а</translation>
    </message>
</context>
<context>
    <name>QIHttp</name>
    <message>
        <source>Connection timed out</source>
        <translation>Вышло время ожидания соединения</translation>
    </message>
    <message>
        <source>Could not locate the file on the server (response: %1)</source>
        <translation>Не удалось обнаружить данный файл на сервере (ответ: %1)</translation>
    </message>
</context>
<context>
    <name>QILabel</name>
    <message>
        <source>&amp;Copy</source>
        <translation>&amp;Копировать</translation>
    </message>
</context>
<context>
    <name>QILabelPrivate</name>
    <message>
        <source>&amp;Copy</source>
        <translation type="obsolete">&amp;Копировать</translation>
    </message>
</context>
<context>
    <name>QIMessageBox</name>
    <message>
        <source>OK</source>
        <translation>ОК</translation>
    </message>
    <message>
        <source>Yes</source>
        <translation>Да</translation>
    </message>
    <message>
        <source>No</source>
        <translation>Нет</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation>Отмена</translation>
    </message>
    <message>
        <source>Ignore</source>
        <translation>Игнорировать</translation>
    </message>
    <message>
        <source>&amp;Details</source>
        <translation>&amp;Детали</translation>
    </message>
    <message>
        <source>&amp;Details (%1 of %2)</source>
        <translation>&amp;Детали (%1 из %2)</translation>
    </message>
</context>
<context>
    <name>QIWidgetValidator</name>
    <message>
        <source>not complete</source>
        <comment>value state</comment>
        <translation>введено не до конца</translation>
    </message>
    <message>
        <source>invalid</source>
        <comment>value state</comment>
        <translation>задано неверно</translation>
    </message>
    <message>
        <source>&lt;qt&gt;The value of the &lt;b&gt;%1&lt;/b&gt; field on the &lt;b&gt;%2&lt;/b&gt; page is %3.&lt;/qt&gt;</source>
        <translation>&lt;qt&gt;Значение поля &lt;b&gt;%1&lt;/b&gt; на странице &lt;b&gt;%2&lt;/b&gt; %3.&lt;/qt&gt;</translation>
    </message>
    <message>
        <source>&lt;qt&gt;One of the values on the &lt;b&gt;%1&lt;/b&gt; page is %2.&lt;/qt&gt;</source>
        <translation>&lt;qt&gt;Одно из значений на странице &lt;b&gt;%1&lt;/b&gt; %2.&lt;/qt&gt;</translation>
    </message>
</context>
<context>
    <name>QIWizardPage</name>
    <message>
        <source>Use the &lt;b&gt;%1&lt;/b&gt; button to go to the next page of the wizard and the &lt;b&gt;%2&lt;/b&gt; button to return to the previous page. You can also press &lt;b&gt;%3&lt;/b&gt; if you want to cancel the execution of this wizard.&lt;/p&gt;</source>
        <translation>Используйте кнопку &lt;b&gt;%1&lt;/b&gt; чтобы перейти к следующей странице мастера, либо кнопку &lt;b&gt;%2&lt;/b&gt; чтобы вернуться к предыдущей. Вы также можете воспользоваться кнопкой &lt;b&gt;%3&lt;/b&gt; если хотите прервать работу мастера вовсе.&lt;/p&gt;</translation>
    </message>
</context>
<context>
    <name>UIActionsPool</name>
    <message>
        <source>&amp;Machine</source>
        <translation>&amp;Машина</translation>
    </message>
    <message>
        <source>&amp;Fullscreen Mode</source>
        <translation type="obsolete">&amp;Полноэкранный режим</translation>
    </message>
    <message>
        <source>Switch to fullscreen mode</source>
        <translation type="obsolete">Переключиться в полноэкранный режим</translation>
    </message>
    <message>
        <source>Seam&amp;less Mode</source>
        <translation type="obsolete">Режим интеграции &amp;дисплея</translation>
    </message>
    <message>
        <source>Switch to seamless desktop integration mode</source>
        <translation type="obsolete">Переключиться в режим интеграции дисплея с рабочим столом</translation>
    </message>
    <message>
        <source>Auto-resize &amp;Guest Display</source>
        <translation>П&amp;одгонять размер экрана гостевой ОС</translation>
    </message>
    <message>
        <source>Automatically resize the guest display when the window is resized (requires Guest Additions)</source>
        <translation>Автоматически подгонять размер экрана гостевой ОС при изменении размеров окна (требуются Дополнения гостевой ОС)</translation>
    </message>
    <message>
        <source>&amp;Adjust Window Size</source>
        <translation>По&amp;догнать размер окна</translation>
    </message>
    <message>
        <source>Adjust window size and position to best fit the guest display</source>
        <translation>Подогнать размер и положение окна под размер экрана гостевой ОС</translation>
    </message>
    <message>
        <source>Disable &amp;Mouse Integration</source>
        <translation>Выключить интеграцию &amp;мыши</translation>
    </message>
    <message>
        <source>Temporarily disable host mouse pointer integration</source>
        <translation>Временно отключить интеграцию указателя мыши</translation>
    </message>
    <message>
        <source>Enable &amp;Mouse Integration</source>
        <translation type="obsolete">Включить интеграцию &amp;мыши</translation>
    </message>
    <message>
        <source>Enable temporarily disabled host mouse pointer integration</source>
        <translation type="obsolete">Включить временно отключенную интеграцию указателя мыши</translation>
    </message>
    <message>
        <source>&amp;Insert Ctrl-Alt-Del</source>
        <translation>Посл&amp;ать Ctrl-Alt-Del</translation>
    </message>
    <message>
        <source>Send the Ctrl-Alt-Del sequence to the virtual machine</source>
        <translation>Послать последовательность клавиш Ctrl-Alt-Del в виртуальную машину</translation>
    </message>
    <message>
        <source>&amp;Insert Ctrl-Alt-Backspace</source>
        <translation>Посла&amp;ть Ctrl-Alt-Backspace</translation>
    </message>
    <message>
        <source>Send the Ctrl-Alt-Backspace sequence to the virtual machine</source>
        <translation>Послать последовательность клавиш Ctrl-Alt-Backspace в виртуальную машину</translation>
    </message>
    <message>
        <source>Take &amp;Snapshot...</source>
        <translation>Сделать с&amp;нимок...</translation>
    </message>
    <message>
        <source>Take a snapshot of the virtual machine</source>
        <translation>Сделать снимок текущего состояния виртуальной машины</translation>
    </message>
    <message>
        <source>Session I&amp;nformation Dialog</source>
        <translation type="obsolete">&amp;Информация о сессии</translation>
    </message>
    <message>
        <source>Show Session Information Dialog</source>
        <translation>Показать диалог с информацией о сессии</translation>
    </message>
    <message>
        <source>&amp;Pause</source>
        <translation>Па&amp;уза</translation>
    </message>
    <message>
        <source>Suspend the execution of the virtual machine</source>
        <translation>Приостановить работу виртуальной машины</translation>
    </message>
    <message>
        <source>R&amp;esume</source>
        <translation type="obsolete">П&amp;родолжить</translation>
    </message>
    <message>
        <source>Resume the execution of the virtual machine</source>
        <translation type="obsolete">Возобновить работу приостановленной виртуальной машины</translation>
    </message>
    <message>
        <source>&amp;Reset</source>
        <translation>С&amp;брос</translation>
    </message>
    <message>
        <source>Reset the virtual machine</source>
        <translation>Послать сигнал Сброс для перезапуска виртуальной машины</translation>
    </message>
    <message>
        <source>ACPI Sh&amp;utdown</source>
        <translation>В&amp;ыключить через ACPI</translation>
    </message>
    <message>
        <source>ACPI S&amp;hutdown</source>
        <translation type="obsolete">В&amp;ыключить через ACPI</translation>
    </message>
    <message>
        <source>Send the ACPI Power Button press event to the virtual machine</source>
        <translation>Послать ACPI-сигнал нажатия кнопки питания в виртуальную машину</translation>
    </message>
    <message>
        <source>&amp;Close...</source>
        <translation>&amp;Закрыть...</translation>
    </message>
    <message>
        <source>Close the virtual machine</source>
        <translation>Закрыть виртуальную машину</translation>
    </message>
    <message>
        <source>&amp;View</source>
        <translation>&amp;Вид</translation>
    </message>
    <message>
        <source>&amp;Devices</source>
        <translation>&amp;Устройства</translation>
    </message>
    <message>
        <source>&amp;CD/DVD Devices</source>
        <translation>&amp;Приводы оптических дисков</translation>
    </message>
    <message>
        <source>&amp;Floppy Devices</source>
        <translation>П&amp;риводы гибких дисков</translation>
    </message>
    <message>
        <source>&amp;USB Devices</source>
        <translation>&amp;Устройства USB</translation>
    </message>
    <message>
        <source>&amp;Network Adapters...</source>
        <translation>&amp;Сетевые адаптеры...</translation>
    </message>
    <message>
        <source>Change the settings of network adapters</source>
        <translation>Открыть диалог для настройки сетевых адаптеров</translation>
    </message>
    <message>
        <source>&amp;Shared Folders...</source>
        <translation>&amp;Общие папки...</translation>
    </message>
    <message>
        <source>Create or modify shared folders</source>
        <translation>Открыть диалог для настройки общих папок</translation>
    </message>
    <message>
        <source>Enable or disable remote desktop (RDP) connections to this machine</source>
        <translation type="obsolete">Разрешить или запретить подключение удаленных клиентов по протоколу RDP к этой машине </translation>
    </message>
    <message>
        <source>&amp;Install Guest Additions...</source>
        <translation>Ус&amp;тановить Дополнения гостевой ОС...</translation>
    </message>
    <message>
        <source>Mount the Guest Additions installation image</source>
        <translation>Подключить установочный образ CD c пакетом Дополнений гостевой ОС</translation>
    </message>
    <message>
        <source>De&amp;bug</source>
        <translation>От&amp;ладка</translation>
    </message>
    <message>
        <source>&amp;Statistics...</source>
        <comment>debug action</comment>
        <translation>&amp;Статистика...</translation>
    </message>
    <message>
        <source>&amp;Command Line...</source>
        <comment>debug action</comment>
        <translation>&amp;Командная строка...</translation>
    </message>
    <message>
        <source>&amp;Logging...</source>
        <comment>debug action</comment>
        <translation type="obsolete">С&amp;бор данных...</translation>
    </message>
    <message>
        <source>&amp;Help</source>
        <translation>Справк&amp;а</translation>
    </message>
    <message>
        <source>Dock Icon</source>
        <translation>Иконка дока</translation>
    </message>
    <message>
        <source>Show Monitor Preview</source>
        <translation>Предпросмотр монитора</translation>
    </message>
    <message>
        <source>Show Application Icon</source>
        <translation>Показать иконку приложения</translation>
    </message>
    <message>
        <source>Enter &amp;Fullscreen Mode</source>
        <translation type="obsolete">Войти в &amp;полноэкранный режим</translation>
    </message>
    <message>
        <source>Exit &amp;Fullscreen Mode</source>
        <translation type="obsolete">Выйти из &amp;полноэкранного режима</translation>
    </message>
    <message>
        <source>Switch to normal mode</source>
        <translation type="obsolete">Переключиться в нормальный (оконный) режим</translation>
    </message>
    <message>
        <source>Enter Seam&amp;less Mode</source>
        <translation type="obsolete">Войти в режим интеграции &amp;дисплея</translation>
    </message>
    <message>
        <source>Exit Seam&amp;less Mode</source>
        <translation type="obsolete">Выйти из режима интеграции &amp;дисплея</translation>
    </message>
    <message>
        <source>Enable &amp;Guest Display Auto-resize</source>
        <translation type="obsolete">Включить автоподстройку &amp;размера экрана</translation>
    </message>
    <message>
        <source>Disable &amp;Guest Display Auto-resize</source>
        <translation type="obsolete">Выключить автоподстройку &amp;размера экрана</translation>
    </message>
    <message>
        <source>Disable automatic resize of the guest display when the window is resized</source>
        <translation type="obsolete">Не совершать подстройку размер экрана гостевой ОС при изменении размеров окна</translation>
    </message>
    <message>
        <source>&amp;Enable Remote Display</source>
        <translation type="obsolete">Включить удалённый &amp;дисплей</translation>
    </message>
    <message>
        <source>Enable remote desktop (RDP) connections to this machine</source>
        <translation>Разрешить подключение удаленных клиентов по протоколу RDP к этой машине</translation>
    </message>
    <message>
        <source>&amp;Disable Remote Display</source>
        <translation type="obsolete">Выключить удалённый &amp;дисплей</translation>
    </message>
    <message>
        <source>Disable remote desktop (RDP) connections to this machine</source>
        <translation type="obsolete">Запретить подключение удаленных клиентов по протоколу RDP к этой машине</translation>
    </message>
    <message>
        <source>Enable &amp;Logging...</source>
        <comment>debug action</comment>
        <translation>Включить с&amp;бор данных...</translation>
    </message>
    <message>
        <source>Disable &amp;Logging...</source>
        <comment>debug action</comment>
        <translation type="obsolete">Выключить с&amp;бор данных...</translation>
    </message>
    <message>
        <source>Switch to &amp;Fullscreen</source>
        <translation>&amp;Полноэкранный режим</translation>
    </message>
    <message>
        <source>Switch between normal and fullscreen mode</source>
        <translation>Переключиться в полноэкранный режим</translation>
    </message>
    <message>
        <source>Switch to Seam&amp;less Mode</source>
        <translation>&amp;Режим интеграции дисплея</translation>
    </message>
    <message>
        <source>Switch between normal and seamless desktop integration mode</source>
        <translation>Переключиться в режим интеграции гостевого дисплея с рабочим столом</translation>
    </message>
    <message>
        <source>Switch to &amp;Scale Mode</source>
        <translation>Р&amp;ежим масштабирования</translation>
    </message>
    <message>
        <source>Switch between normal and scale mode</source>
        <translation>Переключиться в режим масштабирования дисплея гостевой ОС</translation>
    </message>
    <message>
        <source>Session I&amp;nformation</source>
        <translation type="obsolete">&amp;Информация о сессии</translation>
    </message>
    <message>
        <source>Enable R&amp;emote Display</source>
        <translation>Удалённый &amp;дисплей</translation>
    </message>
    <message>
        <source>&amp;Settings...</source>
        <translation>&amp;Свойства...</translation>
    </message>
    <message>
        <source>Manage the virtual machine settings</source>
        <translation>Настроить виртуальную машину</translation>
    </message>
    <message>
        <source>Session I&amp;nformation...</source>
        <translation>&amp;Информация о сессии...</translation>
    </message>
</context>
<context>
    <name>UICloneVMWizard</name>
    <message>
        <source>Clone a virtual machine</source>
        <translation>Копировать виртуальную машину</translation>
    </message>
    <message>
        <source>Clone</source>
        <translation>Копировать</translation>
    </message>
</context>
<context>
    <name>UICloneVMWizardPage1</name>
    <message>
        <source>&lt;p&gt;This wizard will help you to create a clone of your virtual machine.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Данный мастер поможет Вам создать копию Вашей виртуальной машины.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Please choose a name for the new virtual machine:&lt;/p&gt;</source>
        <translation>&lt;p&gt;Пожалуйста, выберите имя для новой виртуальной машины:&lt;/p&gt;</translation>
    </message>
    <message>
        <source>When checked a new unique MAC address will assigned to all configured network cards.</source>
        <translation>Если галочка стоит, всем сетевым адаптерам новой машины будут назначены новые уникальные MAC адреса.</translation>
    </message>
    <message>
        <source>&amp;Reinitialize the MAC address of all network cards</source>
        <translation>&amp;Сгенерировать новые MAC адреса для всех сетевых адаптеров</translation>
    </message>
    <message>
        <source>Welcome to the virtual machine clone wizard</source>
        <translation>Мастер копирования виртуальной машины</translation>
    </message>
    <message>
        <source>%1 Clone</source>
        <translation>Копия %1</translation>
    </message>
</context>
<context>
    <name>UICloneVMWizardPage2</name>
    <message>
        <source>Current machine state</source>
        <translation>Только состояние машины</translation>
    </message>
    <message>
        <source>Current machine and all child states</source>
        <translation>Состояние машины и всех дочерних снимков</translation>
    </message>
    <message>
        <source>All states</source>
        <translation>Состояние машины и всех снимков</translation>
    </message>
    <message>
        <source>Cloning Configuration</source>
        <translation>Конфигурация копирования</translation>
    </message>
    <message>
        <source>Please choose which parts of the virtual machine should be cloned.</source>
        <translation>Пожалуйста уточните, какие части виртуальной машины должны быть скопированы.</translation>
    </message>
    <message>
        <source>If you select &lt;b&gt;Current machine state&lt;/b&gt;, only the current state of the virtual machine is cloned.</source>
        <translation>Если Вы выберите &lt;b&gt;Только состояние машины&lt;/b&gt;, будет скопировано лишь текущее состояние машины.</translation>
    </message>
    <message>
        <source>If you select &lt;b&gt;Current machine and all child states&lt;/b&gt; the current state of the virtual machine and any states of child snapshots are cloned.</source>
        <translation>Если Вы выберите &lt;b&gt;Состояние машины и всех дочерних снимков&lt;/b&gt;, будут скопированы текущее состояние машины и состояния тех снимков, которые являются дочерними для текущего состояния машины.</translation>
    </message>
    <message>
        <source>If you select &lt;b&gt;All states&lt;/b&gt;, the current machine state and all snapshots are cloned.</source>
        <translation>Если Вы выберите &lt;b&gt;Состояние машины и всех снимков&lt;/b&gt;, будут скопированы текущее состояние машины и состояния всех снимков.</translation>
    </message>
</context>
<context>
    <name>UIDescriptionPagePrivate</name>
    <message>
        <source>No description. Press the Edit button below to add it.</source>
        <translation>Описание отсутствует. Чтобы его добавить, нажмите кнопку &lt;b&gt;Изменить&lt;/b&gt; внизу окна.</translation>
    </message>
    <message>
        <source>Edit</source>
        <translation>Изменить</translation>
    </message>
    <message>
        <source>Edit (Ctrl+E)</source>
        <translation>Изменить (Ctrl+E)</translation>
    </message>
</context>
<context>
    <name>UIDetailsPagePrivate</name>
    <message>
        <source>Name</source>
        <comment>details report</comment>
        <translation>Имя</translation>
    </message>
    <message>
        <source>OS Type</source>
        <comment>details report</comment>
        <translation>Тип ОС</translation>
    </message>
    <message>
        <source>Base Memory</source>
        <comment>details report</comment>
        <translation>Оперативная память</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;%1 MB&lt;/nobr&gt;</source>
        <comment>details report</comment>
        <translation>&lt;nobr&gt;%1 МБ&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>Processors</source>
        <comment>details report</comment>
        <translation>Процессоры</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;%1&lt;/nobr&gt;</source>
        <comment>details report</comment>
        <translation>&lt;nobr&gt;%1&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>Boot Order</source>
        <comment>details report</comment>
        <translation>Порядок загрузки</translation>
    </message>
    <message>
        <source>ACPI</source>
        <comment>details report</comment>
        <translation>ACPI</translation>
    </message>
    <message>
        <source>IO APIC</source>
        <comment>details report</comment>
        <translation>IO APIC</translation>
    </message>
    <message>
        <source>BIOS</source>
        <comment>details report</comment>
        <translation>BIOS</translation>
    </message>
    <message>
        <source>VT-x/AMD-V</source>
        <comment>details report</comment>
        <translation>VT-x/AMD-V</translation>
    </message>
    <message>
        <source>Nested Paging</source>
        <comment>details report</comment>
        <translation>Nested Paging</translation>
    </message>
    <message>
        <source>PAE/NX</source>
        <comment>details report</comment>
        <translation>PAE/NX</translation>
    </message>
    <message>
        <source>Acceleration</source>
        <comment>details report</comment>
        <translation>Ускорение</translation>
    </message>
    <message>
        <source>Video Memory</source>
        <comment>details report</comment>
        <translation>Видеопамять</translation>
    </message>
    <message>
        <source>Screens</source>
        <comment>details report</comment>
        <translation>Мониторы</translation>
    </message>
    <message>
        <source>2D Video</source>
        <comment>details report</comment>
        <translation>2D-ускорение видео</translation>
    </message>
    <message>
        <source>3D</source>
        <comment>details report</comment>
        <translation>3D-ускорение</translation>
    </message>
    <message>
        <source>Remote Desktop Server Port</source>
        <comment>details report (VRDE Server)</comment>
        <translation>Порт сервера удалённого дисплея</translation>
    </message>
    <message>
        <source>Remote Desktop Server</source>
        <comment>details report (VRDE Server)</comment>
        <translation>Сервер удалённого дисплея</translation>
    </message>
    <message>
        <source>Disabled</source>
        <comment>details report (VRDE Server)</comment>
        <translation>Выключен</translation>
    </message>
    <message>
        <source>(CD/DVD)</source>
        <translation>(Привод CD/DVD)</translation>
    </message>
    <message>
        <source>Not Attached</source>
        <comment>details report (Storage)</comment>
        <translation>Не подсоединены</translation>
    </message>
    <message>
        <source>Host Driver</source>
        <comment>details report (audio)</comment>
        <translation>Аудиодрайвер</translation>
    </message>
    <message>
        <source>Controller</source>
        <comment>details report (audio)</comment>
        <translation>Контроллер</translation>
    </message>
    <message>
        <source>Disabled</source>
        <comment>details report (audio)</comment>
        <translation>Выключено</translation>
    </message>
    <message>
        <source>Bridged adapter, %1</source>
        <comment>details report (network)</comment>
        <translation>Сетевой мост, &apos;%1&apos;</translation>
    </message>
    <message>
        <source>Internal network, &apos;%1&apos;</source>
        <comment>details report (network)</comment>
        <translation>Внутренняя сеть, &apos;%1&apos;</translation>
    </message>
    <message>
        <source>Host-only adapter, &apos;%1&apos;</source>
        <comment>details report (network)</comment>
        <translation>Виртуальный адаптер хоста, &apos;%1&apos;</translation>
    </message>
    <message>
        <source>VDE network, &apos;%1&apos;</source>
        <comment>details report (network)</comment>
        <translation type="obsolete">VDE-сеть, &apos;%1&apos;</translation>
    </message>
    <message>
        <source>Adapter %1</source>
        <comment>details report (network)</comment>
        <translation>Адаптер %1</translation>
    </message>
    <message>
        <source>Disabled</source>
        <comment>details report (network)</comment>
        <translation>Выключена</translation>
    </message>
    <message>
        <source>Port %1</source>
        <comment>details report (serial ports)</comment>
        <translation>Порт %1</translation>
    </message>
    <message>
        <source>Disabled</source>
        <comment>details report (serial ports)</comment>
        <translation>Выключены</translation>
    </message>
    <message>
        <source>Port %1</source>
        <comment>details report (parallel ports)</comment>
        <translation>Порт %1</translation>
    </message>
    <message>
        <source>Disabled</source>
        <comment>details report (parallel ports)</comment>
        <translation>Выключены</translation>
    </message>
    <message>
        <source>Device Filters</source>
        <comment>details report (USB)</comment>
        <translation>Фильтры устройств</translation>
    </message>
    <message>
        <source>%1 (%2 active)</source>
        <comment>details report (USB)</comment>
        <translation>%1 (%2 активно)</translation>
    </message>
    <message>
        <source>Disabled</source>
        <comment>details report (USB)</comment>
        <translation>Выключено</translation>
    </message>
    <message>
        <source>Shared Folders</source>
        <comment>details report (shared folders)</comment>
        <translation>Общие папки</translation>
    </message>
    <message>
        <source>None</source>
        <comment>details report (shared folders)</comment>
        <translation>Отсутствуют</translation>
    </message>
    <message>
        <source>None</source>
        <comment>details report (description)</comment>
        <translation>Отсутствует</translation>
    </message>
    <message>
        <source>The selected virtual machine is &lt;i&gt;inaccessible&lt;/i&gt;. Please inspect the error message shown below and press the &lt;b&gt;Refresh&lt;/b&gt; button if you want to repeat the accessibility check:</source>
        <translation>Выбранная виртуальная машина &lt;i&gt;недоступна&lt;/i&gt;. Внимательно просмотрите приведенное ниже сообщение об ошибке и нажмите кнопку &lt;b&gt;Обновить&lt;/b&gt;, если Вы хотите повторить проверку доступности:</translation>
    </message>
    <message>
        <source>General</source>
        <comment>details report</comment>
        <translation>Общие</translation>
    </message>
    <message>
        <source>System</source>
        <comment>details report</comment>
        <translation>Система</translation>
    </message>
    <message>
        <source>Preview</source>
        <comment>details report</comment>
        <translation>Превью</translation>
    </message>
    <message>
        <source>Display</source>
        <comment>details report</comment>
        <translation>Дисплей</translation>
    </message>
    <message>
        <source>Storage</source>
        <comment>details report</comment>
        <translation>Носители</translation>
    </message>
    <message>
        <source>Audio</source>
        <comment>details report</comment>
        <translation>Аудио</translation>
    </message>
    <message>
        <source>Network</source>
        <comment>details report</comment>
        <translation>Сеть</translation>
    </message>
    <message>
        <source>Serial Ports</source>
        <comment>details report</comment>
        <translation>COM-порты</translation>
    </message>
    <message>
        <source>Parallel Ports</source>
        <comment>details report</comment>
        <translation>LPT-порты</translation>
    </message>
    <message>
        <source>USB</source>
        <comment>details report</comment>
        <translation>USB</translation>
    </message>
    <message>
        <source>Shared Folders</source>
        <comment>details report</comment>
        <translation>Общие папки</translation>
    </message>
    <message>
        <source>Description</source>
        <comment>details report</comment>
        <translation>Описание</translation>
    </message>
    <message>
        <source>Execution Cap</source>
        <comment>details report</comment>
        <translation>Предел загрузки ЦПУ</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;%1%&lt;/nobr&gt;</source>
        <comment>details report</comment>
        <translation>&lt;nobr&gt;%1%&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>Generic driver, &apos;%1&apos;</source>
        <comment>details report (network)</comment>
        <translation>Универсальный драйвер, &apos;%1&apos;</translation>
    </message>
    <message>
        <source>Generic driver, &apos;%1&apos; {&amp;nbsp;%2&amp;nbsp;}</source>
        <comment>details report (network)</comment>
        <translation>Универсальный драйвер, &apos;%1&apos; {&amp;nbsp;%2&amp;nbsp;}</translation>
    </message>
</context>
<context>
    <name>UIDownloader</name>
    <message>
        <source>The download process has been cancelled by the user.</source>
        <translation type="obsolete">Скачивание файла было отменено пользователем.</translation>
    </message>
    <message>
        <source>The download process has been canceled by the user.</source>
        <translation>Процесс загрузки файла был прерван пользователем.</translation>
    </message>
</context>
<context>
    <name>UIDownloaderAdditions</name>
    <message>
        <source>Cancel</source>
        <translation type="obsolete">Отмена</translation>
    </message>
    <message>
        <source>Downloading the VirtualBox Guest Additions CD image from &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;...&lt;/nobr&gt;</source>
        <translation type="obsolete">Скачивается CD-образ пакета Дополнений гостевой ОС с &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;...&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>Cancel the VirtualBox Guest Additions CD image download</source>
        <translation type="obsolete">Отменить скачивание CD-образа пакета Дополнений гостевой ОС</translation>
    </message>
    <message>
        <source>&lt;p&gt;Failed to save the downloaded file as &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;.&lt;/nobr&gt;&lt;/p&gt;</source>
        <translation>&lt;p&gt;Не удалось сохранить скачанный файл как &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;.&lt;/nobr&gt;&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Select folder to save Guest Additions image to</source>
        <translation>Выберите папку для сохранения образа Дополнений гостевой ОС</translation>
    </message>
</context>
<context>
    <name>UIDownloaderUserManual</name>
    <message>
        <source>Select folder to save User Manual to</source>
        <translation>Выберите каталог для размещения Руководства Пользователя</translation>
    </message>
</context>
<context>
    <name>UIExportApplianceWzd</name>
    <message>
        <source>Select a file to export into</source>
        <translation type="obsolete">Укажите имя файла для экспорта конфигурации</translation>
    </message>
    <message>
        <source>Open Virtualization Format (%1)</source>
        <translation type="obsolete">Открытый Формат Виртуализации (%1)</translation>
    </message>
    <message>
        <source>Appliance</source>
        <translation type="obsolete">Конфигурация</translation>
    </message>
    <message>
        <source>Exporting Appliance ...</source>
        <translation type="obsolete">Экспорт конфигурации ...</translation>
    </message>
    <message>
        <source>Appliance Export Wizard</source>
        <translation>Мастер экспорта конфигураций</translation>
    </message>
    <message>
        <source>Welcome to the Appliance Export Wizard!</source>
        <translation type="obsolete">Добро пожаловать в мастер экспорта конфигурации!</translation>
    </message>
    <message>
        <source>&lt;!DOCTYPE HTML PUBLIC &quot;-//W3C//DTD HTML 4.0//EN&quot; &quot;http://www.w3.org/TR/REC-html40/strict.dtd&quot;&gt;
&lt;html&gt;&lt;head&gt;&lt;meta name=&quot;qrichtext&quot; content=&quot;1&quot; /&gt;&lt;style type=&quot;text/css&quot;&gt;
p, li { white-space: pre-wrap; }
&lt;/style&gt;&lt;/head&gt;&lt;body&gt;
&lt;p style=&quot; margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;&quot;&gt;This wizard will guide you through the process of exporting an appliance. &lt;/p&gt;
&lt;p style=&quot; margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;&quot;&gt;Use the &lt;span style=&quot; font-weight:600;&quot;&gt;Next&lt;/span&gt; button to go the next page of the wizard and the &lt;span style=&quot; font-weight:600;&quot;&gt;Back&lt;/span&gt; button to return to the previous page.&lt;/p&gt;
&lt;p style=&quot; margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;&quot;&gt;Please select the virtual machines that you wish to the appliance. You can select more than one. Please note that these machines have to be turned off before they can be exported.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation type="obsolete">&lt;!DOCTYPE HTML PUBLIC &quot;-//W3C//DTD HTML 4.0//EN&quot; &quot;http://www.w3.org/TR/REC-html40/strict.dtd&quot;&gt;
&lt;html&gt;&lt;head&gt;&lt;meta name=&quot;qrichtext&quot; content=&quot;1&quot; /&gt;&lt;style type=&quot;text/css&quot;&gt;
p, li { white-space: pre-wrap; }
&lt;/style&gt;&lt;/head&gt;&lt;body&gt;
&lt;p style=&quot; margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;&quot;&gt;Этот мастер поможет Вам выполнить экспорт конфигурации группы виртуальных машин. &lt;/p&gt;
&lt;p style=&quot; margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;&quot;&gt;Используйте кнопку &lt;span style=&quot; font-weight:600;&quot;&gt;Далее&lt;/span&gt; для перехода к следующей странице мастера и кнопку &lt;span style=&quot; font-weight:600;&quot;&gt;Назад&lt;/span&gt; для возврата к предыдущей.&lt;/p&gt;
&lt;p style=&quot; margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;&quot;&gt;Пожалуйста укажите одну или несколько виртуальных машин для экспорта. Пожалуйста учтите, что эти машины должны быть остановлены перед началом процесса экспорта.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>&lt; &amp;Back</source>
        <translation type="obsolete">&lt; &amp;Назад</translation>
    </message>
    <message>
        <source>&amp;Next &gt;</source>
        <translation type="obsolete">&amp;Далее &gt;</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation type="obsolete">Отмена</translation>
    </message>
    <message>
        <source>Appliance Export Settings</source>
        <translation type="obsolete">Опции экспорта конфигурации</translation>
    </message>
    <message>
        <source>Here you can change additional configuration values of the selected virtual machines. You can modify most of the properties shown by double-clicking on the items.</source>
        <translation type="obsolete">Здесь Вы можете изменить дополнительные параметры конфигурации выбранных виртуальных машин. Большинство из указанных параметров можно поменять двойным щелчком мыши на выбранном элементе.</translation>
    </message>
    <message>
        <source>Restore Defaults</source>
        <translation>По умолчанию</translation>
    </message>
    <message>
        <source>Please specify a filename into which the appliance information will be written. Currently VirtualBox supports the Open Virtualization Format (OVF).</source>
        <translation type="obsolete">Пожалуйста укажите имя файла, в который будет записана информация о конфигурации. На данный момент VirtualBox поддерживает Открытый Формат Виртуализации (OVF).</translation>
    </message>
    <message>
        <source>&amp;Export &gt;</source>
        <translation type="obsolete">&amp;Экспорт &gt;</translation>
    </message>
    <message>
        <source>Write in legacy OVF 0.9 format for compatibility with other virtualization products.</source>
        <translation type="obsolete">Сохранить в формате OVF 0.9 для совместимости с остальными продуктами виртуализации.</translation>
    </message>
    <message>
        <source>&amp;Write legacy OVF 0.9</source>
        <translation type="obsolete">&amp;Сохранить в формате OVF 0.9</translation>
    </message>
    <message>
        <source>Please choose a filename to export the OVF to.</source>
        <translation type="obsolete">Пожалуйста укажите имя файла для экспорта OVF.</translation>
    </message>
    <message>
        <source>Please complete the additional fields like the username, password and the bucket, and provide a filename for the OVF target.</source>
        <translation type="obsolete">Пожалуйста заполните дополнительные поля такие как имя пользователя, пароль и имя хранилища. В конце укажите имя файла-цели для экспорта OVF.</translation>
    </message>
    <message>
        <source>Please complete the additional fields like the username, password, hostname and the bucket, and provide a filename for the OVF target.</source>
        <translation type="obsolete">Пожалуйста заполните дополнительные поля такие как имя пользователя, пароль, имя хоста и имя хранилища. В конце укажите имя файла-цели для экспорта OVF.</translation>
    </message>
    <message>
        <source>Checking files ...</source>
        <translation type="obsolete">Проверка файлов ...</translation>
    </message>
    <message>
        <source>Removing files ...</source>
        <translation type="obsolete">Удаление файлов ...</translation>
    </message>
    <message>
        <source>Please specify the target for the OVF export. You can choose between a local file system export, uploading the OVF to the Sun Cloud service or an S3 storage server.</source>
        <translation type="obsolete">Пожалуйста укажите точку экспорта OVF. Вы можете экспортировать OVF в локальную файловую систему, а также выгрузить OVF либо на сервер Sun Cloud либо на сервер хранилище S3.</translation>
    </message>
    <message>
        <source>&amp;Local Filesystem </source>
        <translation type="obsolete">&amp;Локальная файловая система</translation>
    </message>
    <message>
        <source>Sun &amp;Cloud</source>
        <translation type="obsolete">С&amp;ервис Sun Cloud</translation>
    </message>
    <message>
        <source>&amp;Simple Storage System (S3)</source>
        <translation type="obsolete">Сервер &amp;хранилище S3</translation>
    </message>
    <message>
        <source>&amp;Username:</source>
        <translation type="obsolete">&amp;Имя пользователя:</translation>
    </message>
    <message>
        <source>&amp;Password:</source>
        <translation type="obsolete">&amp;Пароль:</translation>
    </message>
    <message>
        <source>&amp;File:</source>
        <translation type="obsolete">&amp;Файл:</translation>
    </message>
    <message>
        <source>&amp;Bucket:</source>
        <translation type="obsolete">Х&amp;ранилище:</translation>
    </message>
    <message>
        <source>&amp;Hostname:</source>
        <translation type="obsolete">Имя х&amp;оста:</translation>
    </message>
    <message>
        <source>Export</source>
        <translation>Экспорт</translation>
    </message>
</context>
<context>
    <name>UIExportApplianceWzdPage1</name>
    <message>
        <source>Welcome to the Appliance Export Wizard!</source>
        <translation>Мастер экспорта конфигураций</translation>
    </message>
    <message>
        <source>&lt;p&gt;This wizard will guide you through the process of exporting an appliance.&lt;/p&gt;&lt;p&gt;%1&lt;/p&gt;&lt;p&gt;Please select the virtual machines that should be added to the appliance. You can select more than one. Please note that these machines have to be turned off before they can be exported.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Данный мастер поможет Вам осуществить процесс экспорта конфигураций виртуальных машин.&lt;/p&gt;&lt;p&gt;%1&lt;/p&gt;&lt;p&gt;Пожалуйста выберете виртуальные машины, конфигурации которых должны быть экспортированы. Вы можете выбрать несколько виртуальных машин, но пожалуйста учтите, что выбранные машины должны быть выключены перед тем, как экспортирование станет возможным.&lt;/p&gt;</translation>
    </message>
</context>
<context>
    <name>UIExportApplianceWzdPage2</name>
    <message>
        <source>Here you can change additional configuration values of the selected virtual machines. You can modify most of the properties shown by double-clicking on the items.</source>
        <translation type="obsolete">Здесь Вы можете изменить дополнительные параметры конфигураций выбранных виртуальных машин. Большинство из указанных параметров можно изменить дважды щёлкнув мышью на выбранном элементе.</translation>
    </message>
    <message>
        <source>Appliance Export Settings</source>
        <translation>Опции экспорта конфигураций</translation>
    </message>
    <message>
        <source>Please specify the target for the OVF export. You can choose between a local file system export, uploading the OVF to the Sun Cloud service or an S3 storage server.</source>
        <translation>Пожалуйста укажите точку экспорта OVF. Вы можете экспортировать OVF в локальную файловую систему, а также выгрузить OVF либо на сервер Sun Cloud либо на сервер хранилище S3.</translation>
    </message>
    <message>
        <source>&amp;Local Filesystem </source>
        <translation>&amp;Локальная файловая система</translation>
    </message>
    <message>
        <source>Sun &amp;Cloud</source>
        <translation>С&amp;ервис Sun Cloud</translation>
    </message>
    <message>
        <source>&amp;Simple Storage System (S3)</source>
        <translation>Сервер &amp;хранилище S3</translation>
    </message>
</context>
<context>
    <name>UIExportApplianceWzdPage3</name>
    <message>
        <source>Please specify the target for the OVF export. You can choose between a local file system export, uploading the OVF to the Sun Cloud service or an S3 storage server.</source>
        <translation type="obsolete">Пожалуйста укажите точку экспорта OVF. Вы можете сохранить OVF в локальной файловой системе, а также выгрузить OVF используя сервис Sun Cloud либо на любой другой сервер-хранилище S3.</translation>
    </message>
    <message>
        <source>&amp;Local Filesystem </source>
        <translation type="obsolete">&amp;Локальная файловая система</translation>
    </message>
    <message>
        <source>Sun &amp;Cloud</source>
        <translation type="obsolete">С&amp;ервис Sun Cloud</translation>
    </message>
    <message>
        <source>&amp;Simple Storage System (S3)</source>
        <translation type="obsolete">Сервер &amp;хранилище S3</translation>
    </message>
    <message>
        <source>Appliance Export Settings</source>
        <translation>Опции экспорта конфигураций</translation>
    </message>
    <message>
        <source>&amp;Username:</source>
        <translation>&amp;Имя пользователя:</translation>
    </message>
    <message>
        <source>&amp;Password:</source>
        <translation>&amp;Пароль:</translation>
    </message>
    <message>
        <source>&amp;Hostname:</source>
        <translation>Имя х&amp;оста:</translation>
    </message>
    <message>
        <source>&amp;Bucket:</source>
        <translation>Х&amp;ранилище:</translation>
    </message>
    <message>
        <source>&amp;File:</source>
        <translation>&amp;Файл:</translation>
    </message>
    <message>
        <source>Write in legacy OVF 0.9 format for compatibility with other virtualization products.</source>
        <translation>Сохранить в формате OVF 0.9 для совместимости с остальными программными средствами виртуализации.</translation>
    </message>
    <message>
        <source>&amp;Write legacy OVF 0.9</source>
        <translation>&amp;Сохранить в формате OVF 0.9</translation>
    </message>
    <message>
        <source>Create a Manifest file for automatic data integrity checks on import.</source>
        <translation>Создать Manifest-файл для автоматической проверки целостности при импорте.</translation>
    </message>
    <message>
        <source>Write &amp;Manifest file</source>
        <translation>Создать &amp;Manifest-файл</translation>
    </message>
    <message>
        <source>Appliance</source>
        <translation>Конфигурация</translation>
    </message>
    <message>
        <source>Select a file to export into</source>
        <translation>Укажите имя файла для экспорта конфигурации</translation>
    </message>
    <message>
        <source>Open Virtualization Format Archive (%1)</source>
        <translation>Архив открытого формата виртуализации (%1)</translation>
    </message>
    <message>
        <source>Open Virtualization Format (%1)</source>
        <translation>Открытый формат виртуализации (%1)</translation>
    </message>
    <message>
        <source>Please choose a filename to export the OVF/OVA to. If you use an &lt;i&gt;ova&lt;/i&gt; file name extension, then all the files will be combined into one Open Virtualization Format Archive. If you use an &lt;i&gt;ovf&lt;/i&gt; extension, several files will be written separately. Other extensions are not allowed.</source>
        <translation>Пожалуйста, укажите имя файла для экспорта OVF/OVA. Если Вы выбрали расширением файла &lt;i&gt;ova&lt;/i&gt;, все файлы будут запакованы в один архив открытого формата виртуализации. Если Вы выбрали расширением файла &lt;i&gt;ovf&lt;/i&gt;, несколько отдельных файлов будут записаны независимо друг от друга. Иные расширения файлов недопустимы.</translation>
    </message>
    <message>
        <source>Please complete the additional fields like the username, password and the bucket, and provide a filename for the OVF target.</source>
        <translation>Пожалуйста заполните дополнительные поля такие как имя пользователя, пароль и имя хранилища. В конце укажите имя файла-цели для экспорта OVF.</translation>
    </message>
    <message>
        <source>Please complete the additional fields like the username, password, hostname and the bucket, and provide a filename for the OVF target.</source>
        <translation>Пожалуйста заполните дополнительные поля такие как имя пользователя, пароль, имя хоста и имя хранилища. В конце укажите имя файла-цели для экспорта OVF.</translation>
    </message>
</context>
<context>
    <name>UIExportApplianceWzdPage4</name>
    <message>
        <source>&amp;Username:</source>
        <translation type="obsolete">&amp;Имя пользователя:</translation>
    </message>
    <message>
        <source>&amp;Password:</source>
        <translation type="obsolete">&amp;Пароль:</translation>
    </message>
    <message>
        <source>&amp;Hostname:</source>
        <translation type="obsolete">Имя х&amp;оста:</translation>
    </message>
    <message>
        <source>&amp;Bucket:</source>
        <translation type="obsolete">Х&amp;ранилище:</translation>
    </message>
    <message>
        <source>&amp;File:</source>
        <translation type="obsolete">&amp;Файл:</translation>
    </message>
    <message>
        <source>Write in legacy OVF 0.9 format for compatibility with other virtualization products.</source>
        <translation type="obsolete">Сохранить в формате OVF 0.9 для совместимости с остальными программными средствами виртуализации.</translation>
    </message>
    <message>
        <source>&amp;Write legacy OVF 0.9</source>
        <translation type="obsolete">&amp;Сохранить в формате OVF 0.9</translation>
    </message>
    <message>
        <source>Appliance Export Settings</source>
        <translation>Опции экспорта конфигураций</translation>
    </message>
    <message>
        <source>Appliance</source>
        <translation type="obsolete">Конфигурация</translation>
    </message>
    <message>
        <source>Select a file to export into</source>
        <translation type="obsolete">Укажите имя файла для экспорта конфигураций</translation>
    </message>
    <message>
        <source>Open Virtualization Format (%1)</source>
        <translation type="obsolete">Открытый Формат Виртуализации (%1)</translation>
    </message>
    <message>
        <source>Please choose a filename to export the OVF to.</source>
        <translation type="obsolete">Пожалуйста укажите имя файла для экспорта OVF.</translation>
    </message>
    <message>
        <source>Please complete the additional fields like the username, password and the bucket, and provide a filename for the OVF target.</source>
        <translation type="obsolete">Пожалуйста заполните дополнительные поля такие как имя пользователя, пароль и имя хранилища. В конце укажите имя файла-цели для экспорта OVF.</translation>
    </message>
    <message>
        <source>Please complete the additional fields like the username, password, hostname and the bucket, and provide a filename for the OVF target.</source>
        <translation type="obsolete">Пожалуйста заполните дополнительные поля такие как имя пользователя, пароль, имя хоста и имя хранилища. В конце укажите имя файла-цели для экспорта OVF.</translation>
    </message>
    <message>
        <source>Checking files ...</source>
        <translation>Проверка файлов ...</translation>
    </message>
    <message>
        <source>Removing files ...</source>
        <translation>Удаление файлов ...</translation>
    </message>
    <message>
        <source>Exporting Appliance ...</source>
        <translation>Экспорт конфигураций ...</translation>
    </message>
    <message>
        <source>Here you can change additional configuration values of the selected virtual machines. You can modify most of the properties shown by double-clicking on the items.</source>
        <translation>Здесь Вы можете изменить дополнительные параметры конфигурации выбранных виртуальных машин. Большинство из указанных параметров можно поменять двойным щелчком мыши на выбранном элементе.</translation>
    </message>
</context>
<context>
    <name>UIFirstRunWzd</name>
    <message>
        <source>First Run Wizard</source>
        <translation>Мастер первого запуска</translation>
    </message>
    <message>
        <source>&lt;p&gt;You have started a newly created virtual machine for the first time. This wizard will help you to perform the steps necessary for installing an operating system of your choice onto this virtual machine.&lt;/p&gt;&lt;p&gt;Use the &lt;b&gt;Next&lt;/b&gt; button to go to the next page of the wizard and the &lt;b&gt;Back&lt;/b&gt; button to return to the previous page. You can also press &lt;b&gt;Cancel&lt;/b&gt; if you want to cancel the execution of this wizard.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Вы запустили новую виртуальную машину в первый раз. Этот мастер поможет Вам выполнить шаги, необходимые для установки операционной системы на данную виртуальную машину.&lt;/p&gt;&lt;p&gt;Нажмите кнопку &lt;b&gt;Далее&lt;/b&gt;, чтобы перейти к следующей странице мастера, или кнопку &lt;b&gt;Назад&lt;/b&gt; для возврата на предыдущую страницу. Нажмите кнопку &lt;b&gt;Отмена&lt;/b&gt;, если вы хотите отменить выполнение этого мастера.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Welcome to the First Run Wizard!</source>
        <translation type="obsolete">Мастер первого запуска</translation>
    </message>
    <message>
        <source>&lt;p&gt;Select the type of media you would like to use for installation.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Выберите тип носителя, который Вы бы хотели использовать для установки операционной системы.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Media Type</source>
        <translation type="obsolete">Тип носителя</translation>
    </message>
    <message>
        <source>&amp;CD/DVD-ROM Device</source>
        <translation type="obsolete">&amp;Привод оптических дисков</translation>
    </message>
    <message>
        <source>&amp;Floppy Device</source>
        <translation type="obsolete">П&amp;ривод гибких дисков</translation>
    </message>
    <message>
        <source>&lt;p&gt;Select the media which contains the setup program of the operating system you want to install. This media must be bootable, otherwise the setup program will not be able to start.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Выберите носитель, который содержит программу установки операционной системы, которую Вы хотите установить. Этот носитель должен быть загрузочным, иначе программа установки не сможет начать работу.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Media Source</source>
        <translation type="obsolete">Носитель</translation>
    </message>
    <message>
        <source>&amp;Host Drive</source>
        <translation type="obsolete">&amp;Физический привод </translation>
    </message>
    <message>
        <source>&amp;Image File</source>
        <translation type="obsolete">Ф&amp;айл образа</translation>
    </message>
    <message>
        <source>Select Installation Media</source>
        <translation type="obsolete">Выберите установочный носитель</translation>
    </message>
    <message>
        <source>&lt;p&gt;You have selected the following media to boot from:&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Вы выбрали следующий носитель для загрузки виртуальной машины:&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Summary</source>
        <translation type="obsolete">Итог</translation>
    </message>
    <message>
        <source>CD/DVD-ROM Device</source>
        <translation type="obsolete">Привод оптических дисков</translation>
    </message>
    <message>
        <source>Floppy Device</source>
        <translation type="obsolete">Привод гибких дисков</translation>
    </message>
    <message>
        <source>Host Drive %1</source>
        <translation type="obsolete">Физический привод %1</translation>
    </message>
    <message>
        <source>&lt;p&gt;You have started a newly created virtual machine for the first time. This wizard will help you to perform the steps necessary for booting an operating system of your choice on the virtual machine.&lt;/p&gt;&lt;p&gt;Note that you will not be able to install an operating system into this virtual machine right now because you did not attach any hard disk to it. If this is not what you want, you can cancel the execution of this wizard, select &lt;b&gt;Settings&lt;/b&gt; from the &lt;b&gt;Machine&lt;/b&gt; menu of the main VirtualBox window to access the settings dialog of this machine and change the hard disk configuration.&lt;/p&gt;&lt;p&gt;Use the &lt;b&gt;Next&lt;/b&gt; button to go to the next page of the wizard and the &lt;b&gt;Back&lt;/b&gt; button to return to the previous page. You can also press &lt;b&gt;Cancel&lt;/b&gt; if you want to cancel the execution of this wizard.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Вы запустили новую виртуальную машину в первый раз. Этот мастер поможет Вам выполнить шаги, необходимые для загрузки операционной системы на данную виртуальную машину.&lt;/p&gt;&lt;p&gt;Учтите, что Вы не сможете установить операционную систему на эту виртуальную машину прямо сейчас, потому что Вы не подсоединили к ней ни одного жесткого диска. Если это не то, что Вы хотите, Вы можете отменить выполнение мастера, выбрать &lt;b&gt;Свойства&lt;/b&gt; из меню &lt;b&gt;Машина&lt;/b&gt; главного окна VirtualBox для открытия диалога настроек машины и изменить конфигурацию жестких дисков.&lt;/p&gt;&lt;p&gt;Нажмите кнопку &lt;b&gt;Далее&lt;/b&gt;, чтобы перейти к следующей странице мастера, или кнопку &lt;b&gt;Назад&lt;/b&gt; для возврата на предыдущую страницу. Нажмите кнопку &lt;b&gt;Отмена&lt;/b&gt;, если вы хотите отменить выполнение этого мастера.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Select the type of media you would like to use for booting an operating system.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Выберите тип носителя, который Вы бы хотели использовать для загрузки операционной системы.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Select the media that contains the operating system you want to work with. This media must be bootable, otherwise the operating system will not be able to start.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Выберите носитель, который содержит операционную систему, с которой Вы хотите работать. Этот носитель должен быть загрузочным, иначе операционная система не сможет начать работу.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;You have selected the following media to boot an operating system from:&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Вы выбрали следующий носитель для загрузки операционной системы:&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;If the above is correct, press the &lt;b&gt;Finish&lt;/b&gt; button. Once you press it, the selected media will be mounted on the virtual machine and the machine will start execution.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Если приведенная выше информация верна, нажмите кнопку &lt;b&gt;Готово&lt;/b&gt;. После этого, указанный носитель будет подключен к виртуальной машине, и машина начнет загрузку с этого носителя.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt; &amp;Back</source>
        <translation type="obsolete">&lt; &amp;Назад</translation>
    </message>
    <message>
        <source>&amp;Next &gt;</source>
        <translation type="obsolete">&amp;Далее &gt;</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head&gt;&lt;meta name=&quot;qrichtext&quot; content=&quot;1&quot; /&gt;&lt;style type=&quot;text/css&quot;&gt;
p, li { white-space: pre-wrap; }
&lt;/style&gt;&lt;/head&gt;&lt;body style=&quot; font-family:&apos;Arial&apos;; font-size:9pt; font-weight:400; font-style:normal;&quot;&gt;
&lt;p style=&quot; margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;&quot;&gt;If the above is correct, press the &lt;span style=&quot; font-weight:600;&quot;&gt;Finish&lt;/span&gt; button. Once you press it, the selected media will be temporarily mounted on the virtual machine and the machine will start execution.&lt;/p&gt;
&lt;p style=&quot; margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;&quot;&gt;Please note that when you close the virtual machine, the specified media will be automatically unmounted and the boot device will be set back to the first hard disk.&lt;/p&gt;
&lt;p style=&quot; margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;&quot;&gt;Depending on the type of the setup program, you may need to manually unmount (eject) the media after the setup program reboots the virtual machine, to prevent the installation process from starting again. You can do this by selecting the corresponding &lt;span style=&quot; font-weight:600;&quot;&gt;Unmount...&lt;/span&gt; action in the &lt;span style=&quot; font-weight:600;&quot;&gt;Devices&lt;/span&gt; menu&lt;span style=&quot; font-weight:600;&quot;&gt;.&lt;/span&gt;&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation type="obsolete">&lt;p&gt;Если приведенная выше информация верна, нажмите кнопку &lt;b&gt;Готово&lt;/b&gt;. После этого, указанный носитель будет временно подключен к виртуальной машине, и машина начнет загрузку с этого носителя.&lt;/p&gt;&lt;p&gt;Обратите внимание, что после выключения виртуальной машины, указанный носитель будет автоматически отключен и машина будет переключена на загрузку с первого жесткого диска.&lt;/p&gt;&lt;p&gt;В зависимости от типа программы установки, Вам может потребоваться вручную отключить указанный носитель после того, как программа установки перезагрузит виртуальную машину, для предотвращения повторного запуска процесса установки. Это можно сделать, выбрав соответствующий пункт &lt;b&gt;Отключить...&lt;/b&gt; в меню &lt;b&gt;Устройства&lt;/b&gt;.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&amp;Finish</source>
        <translation type="obsolete">&amp;Готово</translation>
    </message>
    <message>
        <source>Type</source>
        <comment>summary</comment>
        <translation type="obsolete">Тип</translation>
    </message>
    <message>
        <source>Source</source>
        <comment>summary</comment>
        <translation type="obsolete">Носитель</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation type="obsolete">Отмена</translation>
    </message>
    <message>
        <source>&lt;p&gt;If the above is correct, press the &lt;b&gt;Finish&lt;/b&gt; button. Once you press it, the selected media will be temporarily mounted on the virtual machine and the machine will start execution.&lt;/p&gt;&lt;p&gt;Please note that when you close the virtual machine, the specified media will be automatically unmounted and the boot device will be set back to the first hard disk.&lt;/p&gt;&lt;p&gt;Depending on the type of the setup program, you may need to manually unmount (eject) the media after the setup program reboots the virtual machine, to prevent the installation process from starting again. You can do this by selecting the corresponding &lt;b&gt;Unmount...&lt;/b&gt; action in the &lt;b&gt;Devices&lt;/b&gt; menu.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Если вышесказанное верно, нажмите кнопку &lt;b&gt;Готово&lt;/b&gt;. В результате этого действия, выбранное устройство будет временно подключено (диск вставлен) к виртуальной машине, после чего машина будет запущена.&lt;/p&gt;&lt;p&gt;Учтите, что как только Вы закроете виртуальную машину, данное устройство будет автоматически отключено (диск изъят) и машина в дальнейшем будет грузиться с первого из жестких дисков.&lt;/p&gt;&lt;p&gt;В зависимости от типа установочного приложения и для предотвращения его повторного запуска, Вам, возможно, придётся вручную отключить устройство (изъять диск) после того, как установочное приложение перезагрузит виртуальную машину. Вы можете выполнить данное действие выбрав соответствующий пункт &lt;b&gt;Извлечь...&lt;/b&gt; меню &lt;b&gt;Устройства&lt;/b&gt;.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Start</source>
        <translation>Продолжить</translation>
    </message>
</context>
<context>
    <name>UIFirstRunWzdPage1</name>
    <message>
        <source>Welcome to the First Run Wizard!</source>
        <translation>Мастер первого запуска</translation>
    </message>
    <message>
        <source>&lt;p&gt;You have started a newly created virtual machine for the first time. This wizard will help you to perform the steps necessary for installing an operating system of your choice onto this virtual machine.&lt;/p&gt;&lt;p&gt;%1&lt;/p&gt;</source>
        <translation>&lt;p&gt;Вы запустили вновь созданную машину первый раз. Данный мастер поможет Вам осуществить установку выбранной Вами операционной системы на данную машину.&lt;/p&gt;&lt;p&gt;%1&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;You have started a newly created virtual machine for the first time. This wizard will help you to perform the steps necessary for booting an operating system of your choice on the virtual machine.&lt;/p&gt;&lt;p&gt;Note that you will not be able to install an operating system into this virtual machine right now because you did not attach any hard disk to it. If this is not what you want, you can cancel the execution of this wizard, select &lt;b&gt;Settings&lt;/b&gt; from the &lt;b&gt;Machine&lt;/b&gt; menu of the main VirtualBox window to access the settings dialog of this machine and change the hard disk configuration.&lt;/p&gt;&lt;p&gt;%1&lt;/p&gt;</source>
        <translation>&lt;p&gt;Вы запустили вновь созданную машину первый раз. Данный мастер поможет Вам осуществить загрузку выбранной Вами операционной системы на данной машине.&lt;/p&gt;&lt;p&gt;Учтите, что в данный момент Вы не имеете возможности установить операционную систему на данную машину, поскольку Вы не подсоединили к ней ни одного жёсткого диска. Если это не то, что Вы планировали, Вы можете прервать выполнение данного мастера, выбрать &lt;b&gt;Свойства&lt;/b&gt; из меню &lt;b&gt;Машина&lt;/b&gt; главного окна VirtualBox для доступа к настройкам данной машины и исправить конфигурацию жёстких дисков по Вашему усмотрению.&lt;/p&gt;&lt;p&gt;%1&lt;/p&gt;</translation>
    </message>
</context>
<context>
    <name>UIFirstRunWzdPage2</name>
    <message>
        <source>&lt;p&gt;Select the media which contains the setup program of the operating system you want to install. This media must be bootable, otherwise the setup program will not be able to start.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Выберите носитель, который содержит программу установки операционной системы, которую Вы хотите установить. Этот носитель должен быть загрузочным, иначе программа установки не сможет начать работу.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Select the media that contains the operating system you want to work with. This media must be bootable, otherwise the operating system will not be able to start.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Выберите носитель, который содержит операционную систему, с которой Вы хотите работать. Этот носитель должен быть загрузочным, иначе операционная система не сможет начать работу.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Media Source</source>
        <translation>Носитель</translation>
    </message>
    <message>
        <source>Select Installation Media</source>
        <translation>Выберите установочный носитель</translation>
    </message>
</context>
<context>
    <name>UIFirstRunWzdPage3</name>
    <message>
        <source>&lt;p&gt;You have selected the following media to boot from:&lt;/p&gt;</source>
        <translation>&lt;p&gt;Вы выбрали следующий носитель для загрузки виртуальной машины:&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;You have selected the following media to boot an operating system from:&lt;/p&gt;</source>
        <translation>&lt;p&gt;Вы выбрали следующий носитель для загрузки операционной системы:&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;If the above is correct, press the &lt;b&gt;Finish&lt;/b&gt; button. Once you press it, the selected media will be temporarily mounted on the virtual machine and the machine will start execution.&lt;/p&gt;&lt;p&gt;Please note that when you close the virtual machine, the specified media will be automatically unmounted and the boot device will be set back to the first hard disk.&lt;/p&gt;&lt;p&gt;Depending on the type of the setup program, you may need to manually unmount (eject) the media after the setup program reboots the virtual machine, to prevent the installation process from starting again. You can do this by selecting the corresponding &lt;b&gt;Unmount...&lt;/b&gt; action in the &lt;b&gt;Devices&lt;/b&gt; menu.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Если вышесказанное верно, нажмите кнопку &lt;b&gt;Готово&lt;/b&gt;. В результате этого действия, выбранное устройство будет временно подключено (диск вставлен) к виртуальной машине, после чего машина будет запущена.&lt;/p&gt;&lt;p&gt;Учтите, что как только Вы закроете виртуальную машину, данное устройство будет автоматически отключено (диск изъят) и машина в дальнейшем будет грузиться с первого из жестких дисков.&lt;/p&gt;&lt;p&gt;В зависимости от типа установочного приложения и для предотвращения его повторного запуска, Вам, возможно, придётся вручную отключить устройство (изъять диск) после того, как установочное приложение перезагрузит виртуальную машину. Вы можете выполнить данное действие выбрав соответствующий пункт &lt;b&gt;Извлечь...&lt;/b&gt; меню &lt;b&gt;Устройства&lt;/b&gt;.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;If the above is correct, press the &lt;b&gt;Finish&lt;/b&gt; button. Once you press it, the selected media will be mounted on the virtual machine and the machine will start execution.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Если приведенная выше информация верна, нажмите кнопку &lt;b&gt;Готово&lt;/b&gt;. После этого, указанный носитель будет подключен к виртуальной машине, и машина начнет загрузку с этого носителя.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Summary</source>
        <translation>Итог</translation>
    </message>
    <message>
        <source>CD/DVD-ROM Device</source>
        <translation>Привод оптических дисков</translation>
    </message>
    <message>
        <source>Type</source>
        <comment>summary</comment>
        <translation>Тип</translation>
    </message>
    <message>
        <source>Source</source>
        <comment>summary</comment>
        <translation>Носитель</translation>
    </message>
</context>
<context>
    <name>UIGlobalSettingsExtension</name>
    <message>
        <source>&amp;Extension Packages:</source>
        <translation>&amp;Список плагинов:</translation>
    </message>
    <message>
        <source>Lists all installed packages.</source>
        <translation>Список всех установленных плагинов.</translation>
    </message>
    <message>
        <source>Active</source>
        <translation>Активен</translation>
    </message>
    <message>
        <source>Name</source>
        <translation>Имя</translation>
    </message>
    <message>
        <source>Version</source>
        <translation>Версия</translation>
    </message>
    <message>
        <source>Add package</source>
        <translation>Добавить плагин</translation>
    </message>
    <message>
        <source>Remove package</source>
        <translation>Удалить плагин</translation>
    </message>
    <message>
        <source>Select an extension package file</source>
        <translation>Выберите файл, содержащий плагин</translation>
    </message>
    <message>
        <source>Extension package files (%1)</source>
        <translation>Файлы плагинов (%1)</translation>
    </message>
    <message>
        <source>Extensions</source>
        <translation>Плагины</translation>
    </message>
</context>
<context>
    <name>UIGlobalSettingsGeneral</name>
    <message>
        <source>Displays the path to the default virtual machine folder. This folder is used, if not explicitly specified otherwise, when creating new virtual machines.</source>
        <translation>Показывает путь к папке по умолчанию для виртуальных машин. Эта папка используется (если другая папка не указана явным образом) при создании новых виртуальных машин.</translation>
    </message>
    <message>
        <source>Displays the path to the library that provides authentication for Remote Display (VRDP) clients.</source>
        <translation>Показывает путь к библиотеке, обеспечивающей аутентификацию клиентов удаленного дисплея (VRDP).</translation>
    </message>
    <message>
        <source>Default &amp;Hard Disk Folder:</source>
        <translation type="obsolete">Папка для &amp;жестких дисков:</translation>
    </message>
    <message>
        <source>Default &amp;Machine Folder:</source>
        <translation>Папка для &amp;машин:</translation>
    </message>
    <message>
        <source>V&amp;RDP Authentication Library:</source>
        <translation>&amp;Библиотека аутентификации VRDP:</translation>
    </message>
    <message>
        <source>Displays the path to the default hard disk folder. This folder is used, if not explicitly specified otherwise, when adding existing or creating new virtual hard disks.</source>
        <translation type="obsolete">Показывает путь к папке по умолчанию для файлов жестких дисков. Эта папка используется (если другая папка не указана явным образом) при создании или добавлении виртуальных жестких дисков.</translation>
    </message>
    <message>
        <source>When checked, the application will provide an icon with the context menu in the system tray.</source>
        <translation>Если стоит галочка, в области системных уведомлений рабочего стола (системном трее) будет отображаться значок приложения с контекстным меню.</translation>
    </message>
    <message>
        <source>&amp;Show System Tray Icon</source>
        <translation>&amp;Значок в системном трее</translation>
    </message>
    <message>
        <source>When checked, the Dock Icon will reflect the VM window content in realtime.</source>
        <translation type="obsolete">Если стоит галочка, иконка дока будет отображать содержимое окна виртуальной машины в реальном времени.</translation>
    </message>
    <message>
        <source>&amp;Dock Icon Realtime Preview</source>
        <translation type="obsolete">Обновлять иконку &amp;дока</translation>
    </message>
    <message>
        <source>&amp;Auto show Dock and Menubar in fullscreen</source>
        <translation>&amp;Показывать Док и Менюбар в режиме полного экрана</translation>
    </message>
    <message>
        <source>When checked, the host screen saver will be disabled whenever a virtual machine is running.</source>
        <translation>Если стоит галочка, хранитель экрана основной машины (хоста) будет приостановлен на время работы виртуальных машин.</translation>
    </message>
    <message>
        <source>Disable Host &amp;ScreenSaver</source>
        <translation>Отключать &amp;хранитель экрана хоста</translation>
    </message>
</context>
<context>
    <name>UIGlobalSettingsInput</name>
    <message>
        <source>Host &amp;Key:</source>
        <translation>&amp;Хост-клавиша:</translation>
    </message>
    <message>
        <source>Displays the key used as a Host Key in the VM window. Activate the entry field and press a new Host Key. Note that alphanumeric, cursor movement and editing keys cannot be used.</source>
        <translation>Показывает клавишу, используемую в качестве хост-клавиши в окне ВМ. Активируйте поле ввода и нажмите новую хост-клавишу. Нельзя использовать буквенные, цифровые клавиши, клавиши управления курсором и редактирования.</translation>
    </message>
    <message>
        <source>Reset Host Key</source>
        <translation type="obsolete">Сбросить</translation>
    </message>
    <message>
        <source>Resets the key used as a Host Key in the VM window.</source>
        <translation type="obsolete">Сбрасывает назначенную хост-клавишу в значение &apos;не установлено&apos;.</translation>
    </message>
    <message>
        <source>When checked, the keyboard is automatically captured every time the VM window is activated. When the keyboard is captured, all keystrokes (including system ones like Alt-Tab) are directed to the VM.</source>
        <translation>Когда стоит галочка, происходит автоматический захват клавиатуры при каждом переключении в окно ВМ. Когда клавиатура захвачена, все нажатия клавиш (включая системные, такие как Alt-Tab), направляются в ВМ.</translation>
    </message>
    <message>
        <source>&amp;Auto Capture Keyboard</source>
        <translation>А&amp;втозахват клавиатуры</translation>
    </message>
    <message>
        <source>Reset host combination</source>
        <translation>Сбросить хост-комбинацию</translation>
    </message>
    <message>
        <source>Resets the key combination used as the host combination in the VM window.</source>
        <translation>Сбрасывает назначенную хост-комбинацию в значение &apos;не установлено&apos;.</translation>
    </message>
</context>
<context>
    <name>UIGlobalSettingsLanguage</name>
    <message>
        <source> (built-in)</source>
        <comment>Language</comment>
        <translation> (встроенный)</translation>
    </message>
    <message>
        <source>&lt;unavailable&gt;</source>
        <comment>Language</comment>
        <translation>&lt;недоступен&gt;</translation>
    </message>
    <message>
        <source>&lt;unknown&gt;</source>
        <comment>Author(s)</comment>
        <translation>&lt;неизвестно&gt;</translation>
    </message>
    <message>
        <source>Default</source>
        <comment>Language</comment>
        <translation>По умолчанию</translation>
    </message>
    <message>
        <source>Language:</source>
        <translation>Язык:</translation>
    </message>
    <message>
        <source>&amp;Interface Language:</source>
        <translation>&amp;Язык интерфейса:</translation>
    </message>
    <message>
        <source>Lists all available user interface languages. The effective language is written in &lt;b&gt;bold&lt;/b&gt;. Select &lt;i&gt;Default&lt;/i&gt; to reset to the system default language.</source>
        <translation>Перечисляет все доступные языки интерфейса. Активный в настоящий момент язык выделен &lt;b&gt;жирным&lt;/b&gt;. Выберите &lt;i&gt;По умолчанию&lt;/i&gt; для активации языка, используемого в системе по умолчанию.</translation>
    </message>
    <message>
        <source>Name</source>
        <translation>Имя</translation>
    </message>
    <message>
        <source>Id</source>
        <translation>ИД</translation>
    </message>
    <message>
        <source>Language</source>
        <translation>Язык</translation>
    </message>
    <message>
        <source>Author</source>
        <translation>Автор(ы)</translation>
    </message>
    <message>
        <source>Author(s):</source>
        <translation>Автор(ы):</translation>
    </message>
</context>
<context>
    <name>UIGlobalSettingsNetwork</name>
    <message>
        <source>%1 network</source>
        <comment>&lt;adapter name&gt; network</comment>
        <translation type="obsolete">Сеть %1</translation>
    </message>
    <message>
        <source>host IPv4 address of &lt;b&gt;%1&lt;/b&gt; is wrong</source>
        <translation>IPv4 адрес адаптера сети &lt;b&gt;&apos;%1&apos;&lt;/b&gt; задан не корректно</translation>
    </message>
    <message>
        <source>host IPv4 network mask of &lt;b&gt;%1&lt;/b&gt; is wrong</source>
        <translation>IPv4 маска адаптера сети &lt;b&gt;&apos;%1&apos;&lt;/b&gt; задана не корректно</translation>
    </message>
    <message>
        <source>host IPv6 address of &lt;b&gt;%1&lt;/b&gt; is wrong</source>
        <translation>IPv6 адрес адаптера сети &lt;b&gt;&apos;%1&apos;&lt;/b&gt; задан не корректно</translation>
    </message>
    <message>
        <source>DHCP server address of &lt;b&gt;%1&lt;/b&gt; is wrong</source>
        <translation>адрес DHCP сервера сети &lt;b&gt;&apos;%1&apos;&lt;/b&gt; задан не корректно</translation>
    </message>
    <message>
        <source>DHCP server network mask of &lt;b&gt;%1&lt;/b&gt; is wrong</source>
        <translation>маска DHCP сервера сети &lt;b&gt;&apos;%1&apos;&lt;/b&gt; задана не корректно</translation>
    </message>
    <message>
        <source>DHCP lower address bound of &lt;b&gt;%1&lt;/b&gt; is wrong</source>
        <translation>нижняя граница предоставляемых DHCP сервером адресов сети &lt;b&gt;&apos;%1&apos;&lt;/b&gt; задана не корректно</translation>
    </message>
    <message>
        <source>DHCP upper address bound of &lt;b&gt;%1&lt;/b&gt; is wrong</source>
        <translation>верхняя граница предоставляемых DHCP сервером адресов сети &lt;b&gt;&apos;%1&apos;&lt;/b&gt; задана не корректно</translation>
    </message>
    <message>
        <source>Adapter</source>
        <translation>Адаптер</translation>
    </message>
    <message>
        <source>Automatically configured</source>
        <comment>interface</comment>
        <translation>Настроен автоматически</translation>
    </message>
    <message>
        <source>Manually configured</source>
        <comment>interface</comment>
        <translation>Настроен вручную</translation>
    </message>
    <message>
        <source>IPv4 Address</source>
        <translation>IPv4 адрес</translation>
    </message>
    <message>
        <source>Not set</source>
        <comment>address</comment>
        <translation>Не задан</translation>
    </message>
    <message>
        <source>IPv4 Network Mask</source>
        <translation>IPv4 маска сети</translation>
    </message>
    <message>
        <source>Not set</source>
        <comment>mask</comment>
        <translation>Не задана</translation>
    </message>
    <message>
        <source>IPv6 Address</source>
        <translation>IPv6 адрес</translation>
    </message>
    <message>
        <source>IPv6 Network Mask Length</source>
        <translation>IPv6 длина маски сети</translation>
    </message>
    <message>
        <source>Not set</source>
        <comment>length</comment>
        <translation>Не задана</translation>
    </message>
    <message>
        <source>DHCP Server</source>
        <translation>DHCP сервер</translation>
    </message>
    <message>
        <source>Enabled</source>
        <comment>server</comment>
        <translation>Включен</translation>
    </message>
    <message>
        <source>Disabled</source>
        <comment>server</comment>
        <translation>Выключен</translation>
    </message>
    <message>
        <source>Address</source>
        <translation>Адрес</translation>
    </message>
    <message>
        <source>Network Mask</source>
        <translation>Маска сети</translation>
    </message>
    <message>
        <source>Lower Bound</source>
        <translation>Нижняя граница</translation>
    </message>
    <message>
        <source>Not set</source>
        <comment>bound</comment>
        <translation>Не задана</translation>
    </message>
    <message>
        <source>Upper Bound</source>
        <translation>Верхняя граница</translation>
    </message>
    <message>
        <source>&amp;Add host-only network</source>
        <translation>&amp;Добавить виртуальную сеть хоста</translation>
    </message>
    <message>
        <source>&amp;Remove host-only network</source>
        <translation>&amp;Удалить виртуальную сеть хоста</translation>
    </message>
    <message>
        <source>&amp;Edit host-only network</source>
        <translation>&amp;Изменить виртуальную сеть хоста</translation>
    </message>
    <message>
        <source>Performing</source>
        <comment>creating/removing host-only network</comment>
        <translation type="obsolete">Выполняется</translation>
    </message>
    <message>
        <source>&amp;Host-only Networks:</source>
        <translation>&amp;Виртуальные сети хоста:</translation>
    </message>
    <message>
        <source>Lists all available host-only networks.</source>
        <translation>Список всех доступных виртуальных сетей хоста.</translation>
    </message>
    <message>
        <source>Name</source>
        <translation>Имя</translation>
    </message>
    <message>
        <source>New Host-Only Interface</source>
        <translation type="obsolete">Новый интерфейс хоста</translation>
    </message>
    <message>
        <source>Networking</source>
        <translation>Сеть</translation>
    </message>
</context>
<context>
    <name>UIGlobalSettingsNetworkDetails</name>
    <message>
        <source>Host-only Network Details</source>
        <translation>Детали виртуальной сети хоста</translation>
    </message>
    <message>
        <source>&amp;Adapter</source>
        <translation>&amp;Адаптер</translation>
    </message>
    <message>
        <source>Manual &amp;Configuration</source>
        <translation>&amp;Ручная конфигурация</translation>
    </message>
    <message>
        <source>Use manual configuration for this host-only network adapter.</source>
        <translation>Настроить виртуальный адаптер сети хоста вручную.</translation>
    </message>
    <message>
        <source>&amp;IPv4 Address:</source>
        <translation>IPv4 &amp;адрес:</translation>
    </message>
    <message>
        <source>Displays the host IPv4 address for this adapter.</source>
        <translation>Отображает IPv4 адрес адаптера хоста данной виртуальной сети.</translation>
    </message>
    <message>
        <source>IPv4 Network &amp;Mask:</source>
        <translation>IPv4 &amp;маска сети:</translation>
    </message>
    <message>
        <source>Displays the host IPv4 network mask for this adapter.</source>
        <translation>Отображает IPv4 маску адаптера хоста данной виртуальной сети.</translation>
    </message>
    <message>
        <source>I&amp;Pv6 Address:</source>
        <translation>IPv6 а&amp;дрес:</translation>
    </message>
    <message>
        <source>Displays the host IPv6 address for this adapter if IPv6 is supported.</source>
        <translation>Отображает IPv6 адрес адаптера хоста данной виртуальной сети, если IPv6 поддерживается.</translation>
    </message>
    <message>
        <source>IPv6 Network Mask &amp;Length:</source>
        <translation>IPv6 д&amp;лина маски сети:</translation>
    </message>
    <message>
        <source>Displays the host IPv6 network mask prefix length for this adapter if IPv6 is supported.</source>
        <translation>Отображает длину IPv6 маски адаптера хоста данной виртуальной сети, если IPv6 поддерживается.</translation>
    </message>
    <message>
        <source>&amp;DHCP Server</source>
        <translation>DHCP &amp;сервер</translation>
    </message>
    <message>
        <source>&amp;Enable Server</source>
        <translation>&amp;Включить сервер</translation>
    </message>
    <message>
        <source>Indicates whether the DHCP Server is enabled on machine startup or not.</source>
        <translation>Отображает запускается ли DHCP сервер при старте машины.</translation>
    </message>
    <message>
        <source>Server Add&amp;ress:</source>
        <translation>А&amp;дрес сервера:</translation>
    </message>
    <message>
        <source>Displays the address of the DHCP server servicing the network associated with this host-only adapter.</source>
        <translation>Отображает адрес DHCP сервера, обслуживающего виртуальную сеть хоста, связанную с данным сетевым адаптером.</translation>
    </message>
    <message>
        <source>Server &amp;Mask:</source>
        <translation>&amp;Маска сети сервера:</translation>
    </message>
    <message>
        <source>Displays the network mask of the DHCP server servicing the network associated with this host-only adapter.</source>
        <translation>Отображает маску сети DHCP сервера, обслуживающего виртуальную сеть хоста, связанную с данным сетевым адаптером.</translation>
    </message>
    <message>
        <source>&amp;Lower Address Bound:</source>
        <translation>&amp;Нижняя граница адресов:</translation>
    </message>
    <message>
        <source>Displays the lower address bound offered by the DHCP server servicing the network associated with this host-only adapter.</source>
        <translation>Отображает нижнюю границу, предоставляемую DHCP сервером, обслуживающим виртуальную сеть хоста, связанную с данным сетевым адаптером.</translation>
    </message>
    <message>
        <source>&amp;Upper Address Bound:</source>
        <translation>В&amp;ерхняя граница адресов:</translation>
    </message>
    <message>
        <source>Displays the upper address bound offered by the DHCP server servicing the network associated with this host-only adapter.</source>
        <translation>Отображает верхнюю границу, предоставляемую DHCP сервером, обслуживающим виртуальную сеть хоста, связанную с данным сетевым адаптером.</translation>
    </message>
</context>
<context>
    <name>UIGlobalSettingsProxy</name>
    <message>
        <source>When checked, VirtualBox will use the proxy settings supplied for tasks like downloading Guest Additions from the network or checking for updates.</source>
        <translation>Если стоит галочка, VirtualBox будет использовать данные настройки для работы с прокси-сервером в целях загрузки гостевых дополнений и проверки обновлений.</translation>
    </message>
    <message>
        <source>&amp;Enable proxy</source>
        <translation>&amp;Использовать прокси-сервер</translation>
    </message>
    <message>
        <source>Ho&amp;st:</source>
        <translation>А&amp;дрес прокси-сервера:</translation>
    </message>
    <message>
        <source>Changes the proxy host.</source>
        <translation>Задаёт адрес прокси-сервера.</translation>
    </message>
    <message>
        <source>&amp;Port:</source>
        <translation>&amp;Порт:</translation>
    </message>
    <message>
        <source>Changes the proxy port.</source>
        <translation>Задаёт порт прокси-сервера.</translation>
    </message>
    <message>
        <source>When checked the authentication supplied will be used with the proxy server.</source>
        <translation>Если стоит галочка, VirtualBox будет использовать данные настройки аутентификации для работы с прокси-сервером.</translation>
    </message>
    <message>
        <source>&amp;Use authentication</source>
        <translation>И&amp;спользовать аутентификацию</translation>
    </message>
    <message>
        <source>User &amp;name:</source>
        <translation>Им&amp;я пользователя:</translation>
    </message>
    <message>
        <source>Changes the user name used for authentication.</source>
        <translation>Задаёт имя пользователя для аутентификации на прокси-сервере.</translation>
    </message>
    <message>
        <source>Pass&amp;word:</source>
        <translation>Па&amp;роль:</translation>
    </message>
    <message>
        <source>Changes the password used for authentication.</source>
        <translation>Задаёт пароль пользователя для аутентификации на прокси-сервере.</translation>
    </message>
</context>
<context>
    <name>UIGlobalSettingsUpdate</name>
    <message>
        <source>When checked, the application will periodically connect to the VirtualBox website and check whether a new VirtualBox version is available.</source>
        <translation>Если стоит галочка, программа будет периодически подключаться к веб-сайту VirtualBox и проверять наличие новой версии.</translation>
    </message>
    <message>
        <source>&amp;Check for updates</source>
        <translation>&amp;Проверять обновления</translation>
    </message>
    <message>
        <source>&amp;Once per:</source>
        <translation>С &amp;интервалом в:</translation>
    </message>
    <message>
        <source>Specifies how often the new version check should be performed. Note that if you want to completely disable this check, just clear the above check box.</source>
        <translation>Указывает, как часто нужно производить проверку наличия новой версии. Если Вы хотите полностью отключить такую проверку, просто уберите расположенную выше галочку.</translation>
    </message>
    <message>
        <source>Next Check:</source>
        <translation>Следующая проверка:</translation>
    </message>
    <message>
        <source>Check for:</source>
        <translation>Искать:</translation>
    </message>
    <message>
        <source>&lt;p&gt;Choose this if you only wish to be notified about stable updates to VirtualBox.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Выберете этот пункт, если хотите быть информированы лишь о стабильных релизных версиях VirtualBox.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&amp;Stable release versions</source>
        <translation>&amp;Стабильные релизные версии</translation>
    </message>
    <message>
        <source>&lt;p&gt;Choose this if you wish to be notified about all new VirtualBox releases.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Выберете этот пункт, если хотите быть информированы о всех релизных версиях VirtualBox.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&amp;All new releases</source>
        <translation>&amp;Все релизные версии</translation>
    </message>
    <message>
        <source>&lt;p&gt;Choose this to be notified about all new VirtualBox releases and pre-release versions of VirtualBox.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Выберете этот пункт, если хотите быть информированы о всех релизных и тестовых версиях VirtualBox.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>All new releases and &amp;pre-releases</source>
        <translation>Вс&amp;е релизные и тестовые версии</translation>
    </message>
</context>
<context>
    <name>UIHelpButton</name>
    <message>
        <source>&amp;Help</source>
        <translation>Справк&amp;а</translation>
    </message>
</context>
<context>
    <name>UIHotKeyEditor</name>
    <message>
        <source>Left </source>
        <translation>Левый </translation>
    </message>
    <message>
        <source>Right </source>
        <translation>Правый </translation>
    </message>
    <message>
        <source>Left Shift</source>
        <translation type="obsolete">Левый Shift</translation>
    </message>
    <message>
        <source>Right Shift</source>
        <translation type="obsolete">Правый Shift</translation>
    </message>
    <message>
        <source>Left Ctrl</source>
        <translation type="obsolete">Левый Ctrl</translation>
    </message>
    <message>
        <source>Right Ctrl</source>
        <translation type="obsolete">Правый Ctrl</translation>
    </message>
    <message>
        <source>Left Alt</source>
        <translation type="obsolete">Левый Alt</translation>
    </message>
    <message>
        <source>Right Alt</source>
        <translation>Правый Alt</translation>
    </message>
    <message>
        <source>Left WinKey</source>
        <translation>Левая Win-клавиша</translation>
    </message>
    <message>
        <source>Right WinKey</source>
        <translation>Правая Win-клавиша</translation>
    </message>
    <message>
        <source>Menu key</source>
        <translation>Клавиша Menu</translation>
    </message>
    <message>
        <source>Alt Gr</source>
        <translation>Alt Gr</translation>
    </message>
    <message>
        <source>Caps Lock</source>
        <translation>Caps Lock</translation>
    </message>
    <message>
        <source>Scroll Lock</source>
        <translation>Scroll Lock</translation>
    </message>
    <message>
        <source>&lt;key_%1&gt;</source>
        <translation>&lt;клавиша_%1&gt;</translation>
    </message>
    <message>
        <source>F1</source>
        <translation type="obsolete">F1</translation>
    </message>
    <message>
        <source>F2</source>
        <translation type="obsolete">F2</translation>
    </message>
    <message>
        <source>F3</source>
        <translation type="obsolete">F3</translation>
    </message>
    <message>
        <source>F4</source>
        <translation type="obsolete">F4</translation>
    </message>
    <message>
        <source>F5</source>
        <translation type="obsolete">F5</translation>
    </message>
    <message>
        <source>F6</source>
        <translation type="obsolete">F6</translation>
    </message>
    <message>
        <source>F7</source>
        <translation type="obsolete">F7</translation>
    </message>
    <message>
        <source>F8</source>
        <translation type="obsolete">F8</translation>
    </message>
    <message>
        <source>F9</source>
        <translation type="obsolete">F9</translation>
    </message>
    <message>
        <source>F10</source>
        <translation type="obsolete">F10</translation>
    </message>
    <message>
        <source>F11</source>
        <translation type="obsolete">F11</translation>
    </message>
    <message>
        <source>F12</source>
        <translation type="obsolete">F12</translation>
    </message>
    <message>
        <source>F13</source>
        <translation type="obsolete">F13</translation>
    </message>
    <message>
        <source>F14</source>
        <translation type="obsolete">F14</translation>
    </message>
    <message>
        <source>F15</source>
        <translation type="obsolete">F15</translation>
    </message>
    <message>
        <source>F16</source>
        <translation type="obsolete">F16</translation>
    </message>
    <message>
        <source>F17</source>
        <translation type="obsolete">F17</translation>
    </message>
    <message>
        <source>F18</source>
        <translation type="obsolete">F18</translation>
    </message>
    <message>
        <source>F19</source>
        <translation type="obsolete">F19</translation>
    </message>
    <message>
        <source>F20</source>
        <translation type="obsolete">F20</translation>
    </message>
    <message>
        <source>F21</source>
        <translation type="obsolete">F21</translation>
    </message>
    <message>
        <source>F22</source>
        <translation type="obsolete">F22</translation>
    </message>
    <message>
        <source>F23</source>
        <translation type="obsolete">F23</translation>
    </message>
    <message>
        <source>F24</source>
        <translation type="obsolete">F24</translation>
    </message>
    <message>
        <source>None</source>
        <translation>Не установлено</translation>
    </message>
</context>
<context>
    <name>UIImportApplianceWzd</name>
    <message>
        <source>Select an appliance to import</source>
        <translation type="obsolete">Укажите файл конфигурации для импорта</translation>
    </message>
    <message>
        <source>Open Virtualization Format (%1)</source>
        <translation type="obsolete">Открытый Формат Виртуализации (%1)</translation>
    </message>
    <message>
        <source>Appliance Import Wizard</source>
        <translation>Мастер импорта конфигураций</translation>
    </message>
    <message>
        <source>Welcome to the Appliance Import Wizard!</source>
        <translation type="obsolete">Добро пожаловать в мастер импорта конфигурации!</translation>
    </message>
    <message>
        <source>&lt;!DOCTYPE HTML PUBLIC &quot;-//W3C//DTD HTML 4.0//EN&quot; &quot;http://www.w3.org/TR/REC-html40/strict.dtd&quot;&gt;
&lt;html&gt;&lt;head&gt;&lt;meta name=&quot;qrichtext&quot; content=&quot;1&quot; /&gt;&lt;style type=&quot;text/css&quot;&gt;
p, li { white-space: pre-wrap; }
&lt;/style&gt;&lt;/head&gt;&lt;body&gt;
&lt;p style=&quot; margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;&quot;&gt;This wizard will guide you through importing an appliance. &lt;/p&gt;
&lt;p style=&quot; margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;&quot;&gt;Use the &lt;span style=&quot; font-weight:600;&quot;&gt;Next&lt;/span&gt; button to go the next page of the wizard and the &lt;span style=&quot; font-weight:600;&quot;&gt;Back&lt;/span&gt; button to return to the previous page.&lt;/p&gt;
&lt;p style=&quot; margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;&quot;&gt;VirtualBox currently supports importing appliances saved in the Open Virtualization Format (OVF). To continue, select the file to import below:&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation type="obsolete">&lt;!DOCTYPE HTML PUBLIC &quot;-//W3C//DTD HTML 4.0//EN&quot; &quot;http://www.w3.org/TR/REC-html40/strict.dtd&quot;&gt;
&lt;html&gt;&lt;head&gt;&lt;meta name=&quot;qrichtext&quot; content=&quot;1&quot; /&gt;&lt;style type=&quot;text/css&quot;&gt;
p, li { white-space: pre-wrap; }
&lt;/style&gt;&lt;/head&gt;&lt;body&gt;
&lt;p style=&quot; margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;&quot;&gt;Этот мастер поможет Вам выполнить импорт конфигурации группы виртуальных машин. &lt;/p&gt;
&lt;p style=&quot; margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;&quot;&gt;Используйте кнопку &lt;span style=&quot; font-weight:600;&quot;&gt;Далее&lt;/span&gt; для перехода к следующей странице мастера и кнопку &lt;span style=&quot; font-weight:600;&quot;&gt;Назад&lt;/span&gt; для возврата к предыдущей.&lt;/p&gt;
&lt;p style=&quot; margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;&quot;&gt;Для импорта конфигурации Вам необходимо выбрать файл с её описанием. В настоящий момент VirtualBox поддерживает Открытый Формат Виртуализации (OVF). Чтобы продолжить, выберите файл для импорта:&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>&lt; &amp;Back</source>
        <translation type="obsolete">&lt; &amp;Назад</translation>
    </message>
    <message>
        <source>&amp;Next &gt;</source>
        <translation type="obsolete">&amp;Далее &gt;</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation type="obsolete">Отмена</translation>
    </message>
    <message>
        <source>Appliance Import Settings</source>
        <translation type="obsolete">Опции импорта конфигурации</translation>
    </message>
    <message>
        <source>These are the virtual machines contained in the appliance and the suggested settings of the imported VirtualBox machines. You can change many of the properties shown by double-clicking on the items and disable others using the check boxes below.</source>
        <translation type="obsolete">Далее перечислены виртуальные машины и их устройства, описанные в конфигурации, для импорта в VirtualBox. Большинство из указанных параметров можно изменить двойным щелчком мыши на выбранном элементе, либо отключить используя соответствующие галочки.</translation>
    </message>
    <message>
        <source>Restore Defaults</source>
        <translation>По умолчанию</translation>
    </message>
    <message>
        <source>&amp;Import &gt;</source>
        <translation type="obsolete">&amp;Импорт &gt;</translation>
    </message>
    <message>
        <source>Import</source>
        <translation>Импорт</translation>
    </message>
</context>
<context>
    <name>UIImportApplianceWzdPage1</name>
    <message>
        <source>Select an appliance to import</source>
        <translation>Укажите файл конфигураций для импорта</translation>
    </message>
    <message>
        <source>Open Virtualization Format (%1)</source>
        <translation>Открытый Формат Виртуализации (%1)</translation>
    </message>
    <message>
        <source>Welcome to the Appliance Import Wizard!</source>
        <translation>Мастер импорта конфигураций</translation>
    </message>
    <message>
        <source>&lt;p&gt;This wizard will guide you through importing an appliance.&lt;/p&gt;&lt;p&gt;%1&lt;/p&gt;&lt;p&gt;VirtualBox currently supports importing appliances saved in the Open Virtualization Format (OVF). To continue, select the file to import below:&lt;/p&gt;</source>
        <translation>&lt;p&gt;Данный мастер поможет Вам осуществить процесс импорта конфигураций.&lt;/p&gt;&lt;p&gt;%1&lt;/p&gt;&lt;p&gt;VirtualBox в настоящий момент поддерживает импорт конфигураций, сохранённых в Открытом Виртуализационном Формате (OVF). Для того чтобы продолжить, Вам необходимо выбрать файл для импорта:&lt;/p&gt;</translation>
    </message>
</context>
<context>
    <name>UIImportApplianceWzdPage2</name>
    <message>
        <source>These are the virtual machines contained in the appliance and the suggested settings of the imported VirtualBox machines. You can change many of the properties shown by double-clicking on the items and disable others using the check boxes below.</source>
        <translation>Далее перечислены виртуальные машины и их устройства, описанные в конфигурации, для импорта в VirtualBox. Большинство из указанных параметров можно изменить дважды щёлкнув мышью на выбранном элементе, либо отключить используя соответствующие галочки.</translation>
    </message>
    <message>
        <source>Appliance Import Settings</source>
        <translation>Опции импорта конфигураций</translation>
    </message>
</context>
<context>
    <name>UIImportLicenseViewer</name>
    <message>
        <source>&lt;b&gt;The virtual system &quot;%1&quot; requires that you agree to the terms and conditions of the software license agreement shown below.&lt;/b&gt;&lt;br /&gt;&lt;br /&gt;Click &lt;b&gt;Agree&lt;/b&gt; to continue or click &lt;b&gt;Disagree&lt;/b&gt; to cancel the import.</source>
        <translation>&lt;b&gt;Виртуальной системе &quot;%1&quot; требуется, что бы Вы приняли постановления и условия лицензионного соглашения на программное обеспечение, указанные далее.&lt;/b&gt;&lt;br /&gt;&lt;br /&gt;Нажмите &lt;b&gt;Согласен&lt;/b&gt; для продолжения либо &lt;b&gt;Отказываюсь&lt;/b&gt; для отмены импорта.</translation>
    </message>
    <message>
        <source>Software License Agreement</source>
        <translation>Лицензионное соглашение на программное обеспечение</translation>
    </message>
    <message>
        <source>&amp;Disagree</source>
        <translation>&amp;Отказываюсь</translation>
    </message>
    <message>
        <source>&amp;Agree</source>
        <translation>&amp;Принимаю</translation>
    </message>
    <message>
        <source>&amp;Print...</source>
        <translation>П&amp;ечать...</translation>
    </message>
    <message>
        <source>&amp;Save...</source>
        <translation>&amp;Сохранить...</translation>
    </message>
    <message>
        <source>Text (*.txt)</source>
        <translation>Текстовый файл (*.txt)</translation>
    </message>
    <message>
        <source>Save license to file...</source>
        <translation>Сохранить лицензию в файл...</translation>
    </message>
</context>
<context>
    <name>UIIndicatorsPool</name>
    <message>
        <source>&lt;p style=&apos;white-space:pre&apos;&gt;&lt;nobr&gt;Indicates the activity of the virtual hard disks:&lt;/nobr&gt;%1&lt;/p&gt;</source>
        <comment>HDD tooltip</comment>
        <translation>&lt;p style=&apos;white-space:pre&apos;&gt;&lt;nobr&gt;Отображает активность виртуальных жёстких дисков:&lt;/nobr&gt;%1&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p style=&apos;white-space:pre&apos;&gt;&lt;nobr&gt;Indicates the activity of the CD/DVD devices:&lt;/nobr&gt;%1&lt;/p&gt;</source>
        <comment>CD/DVD tooltip</comment>
        <translation>&lt;p style=&apos;white-space:pre&apos;&gt;&lt;nobr&gt;Отображает активность приводов оптических дисков:&lt;/nobr&gt;%1&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p style=&apos;white-space:pre&apos;&gt;&lt;nobr&gt;Indicates the activity of the floppy devices:&lt;/nobr&gt;%1&lt;/p&gt;</source>
        <comment>FD tooltip</comment>
        <translation>&lt;p style=&apos;white-space:pre&apos;&gt;&lt;nobr&gt;Отображает активность приводов гибких дисков:&lt;/nobr&gt;%1&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p style=&apos;white-space:pre&apos;&gt;&lt;nobr&gt;Indicates the activity of the network interfaces:&lt;/nobr&gt;%1&lt;/p&gt;</source>
        <comment>Network adapters tooltip</comment>
        <translation>&lt;p style=&apos;white-space:pre&apos;&gt;&lt;nobr&gt;Отображает активность сетевых адаптеров:&lt;/nobr&gt;%1&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;br&gt;&lt;nobr&gt;&lt;b&gt;Adapter %1 (%2)&lt;/b&gt;: %3 cable %4&lt;/nobr&gt;</source>
        <comment>Network adapters tooltip</comment>
        <translation>&lt;br&gt;&lt;nobr&gt;&lt;b&gt;Адаптер %1 (%2)&lt;/b&gt;: %3 кабель %4&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>connected</source>
        <comment>Network adapters tooltip</comment>
        <translation>подключен</translation>
    </message>
    <message>
        <source>disconnected</source>
        <comment>Network adapters tooltip</comment>
        <translation>отключен</translation>
    </message>
    <message>
        <source>&lt;br&gt;&lt;nobr&gt;&lt;b&gt;All network adapters are disabled&lt;/b&gt;&lt;/nobr&gt;</source>
        <comment>Network adapters tooltip</comment>
        <translation>&lt;br&gt;&lt;nobr&gt;&lt;b&gt;Все сетевые адапреты выключены&lt;/b&gt;&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;p style=&apos;white-space:pre&apos;&gt;&lt;nobr&gt;Indicates the activity of the attached USB devices:&lt;/nobr&gt;%1&lt;/p&gt;</source>
        <comment>USB device tooltip</comment>
        <translation>&lt;p style=&apos;white-space:pre&apos;&gt;&lt;nobr&gt;Отображает активность подсоединенных USB устройств:&lt;/nobr&gt;%1&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;br&gt;&lt;nobr&gt;&lt;b&gt;No USB devices attached&lt;/b&gt;&lt;/nobr&gt;</source>
        <comment>USB device tooltip</comment>
        <translation>&lt;br&gt;&lt;nobr&gt;&lt;b&gt;USB-устройства не подсоединены&lt;/b&gt;&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;br&gt;&lt;nobr&gt;&lt;b&gt;USB Controller is disabled&lt;/b&gt;&lt;/nobr&gt;</source>
        <comment>USB device tooltip</comment>
        <translation>&lt;br&gt;&lt;nobr&gt;&lt;b&gt;Контроллер USB выключен&lt;/b&gt;&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;p style=&apos;white-space:pre&apos;&gt;&lt;nobr&gt;Indicates the activity of the machine&apos;s shared folders:&lt;/nobr&gt;%1&lt;/p&gt;</source>
        <comment>Shared folders tooltip</comment>
        <translation>&lt;p style=&apos;white-space:pre&apos;&gt;&lt;nobr&gt;Отображает активность общих папок машины:&lt;/nobr&gt;%1&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;br&gt;&lt;nobr&gt;&lt;b&gt;No shared folders&lt;/b&gt;&lt;/nobr&gt;</source>
        <comment>Shared folders tooltip</comment>
        <translation>&lt;br&gt;&lt;nobr&gt;&lt;b&gt;Нет общих папок&lt;/b&gt;&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>Indicates whether the Remote Display (VRDP Server) is enabled (&lt;img src=:/vrdp_16px.png/&gt;) or not (&lt;img src=:/vrdp_disabled_16px.png/&gt;).</source>
        <translation type="obsolete">Показывает, включен удаленный дисплей (VRDP-сервер) (&lt;img src=:/vrdp_16px.png/&gt;) или нет (&lt;img src=:/vrdp_disabled_16px.png/&gt;).</translation>
    </message>
    <message>
        <source>&lt;hr&gt;The VRDP Server is listening on port %1</source>
        <translation type="obsolete">&lt;hr&gt;VRDP-сервер ожидает соединений на порту %1</translation>
    </message>
    <message>
        <source>Indicates the status of the hardware virtualization features used by this virtual machine:&lt;br&gt;&lt;nobr&gt;&lt;b&gt;%1:&lt;/b&gt;&amp;nbsp;%2&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;&lt;b&gt;%3:&lt;/b&gt;&amp;nbsp;%4&lt;/nobr&gt;</source>
        <comment>Virtualization Stuff LED</comment>
        <translation type="obsolete">Показывает статус опций аппаратной виртуализации используемых виртуальной машиной:&lt;br&gt;&lt;nobr&gt;&lt;b&gt;%1:&lt;/b&gt;&amp;nbsp;%2&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;&lt;b&gt;%3:&lt;/b&gt;&amp;nbsp;%4&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;br&gt;&lt;nobr&gt;&lt;b&gt;%1:&lt;/b&gt;&amp;nbsp;%2&lt;/nobr&gt;</source>
        <comment>Virtualization Stuff LED</comment>
        <translation>&lt;br&gt;&lt;nobr&gt;&lt;b&gt;%1:&lt;/b&gt;&amp;nbsp;%2&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>Indicates whether the host mouse pointer is captured by the guest OS:&lt;br&gt;&lt;nobr&gt;&lt;img src=:/mouse_disabled_16px.png/&gt;&amp;nbsp;&amp;nbsp;pointer is not captured&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;&lt;img src=:/mouse_16px.png/&gt;&amp;nbsp;&amp;nbsp;pointer is captured&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;&lt;img src=:/mouse_seamless_16px.png/&gt;&amp;nbsp;&amp;nbsp;mouse integration (MI) is On&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;&lt;img src=:/mouse_can_seamless_16px.png/&gt;&amp;nbsp;&amp;nbsp;MI is Off, pointer is captured&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;&lt;img src=:/mouse_can_seamless_uncaptured_16px.png/&gt;&amp;nbsp;&amp;nbsp;MI is Off, pointer is not captured&lt;/nobr&gt;&lt;br&gt;Note that the mouse integration feature requires Guest Additions to be installed in the guest OS.</source>
        <translation>Показывает, захвачен ли указатель мыши основного ПК в гостевой ОС:&lt;br&gt;&lt;nobr&gt;&lt;img src=:/mouse_disabled_16px.png/&gt;&amp;nbsp;&amp;nbsp;указатель не захвачен&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;&lt;img src=:/mouse_16px.png/&gt;&amp;nbsp;&amp;nbsp;указатель захвачен&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;&lt;img src=:/mouse_seamless_16px.png/&gt;&amp;nbsp;&amp;nbsp;интеграция мыши (ИМ) включена&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;&lt;img src=:/mouse_can_seamless_16px.png/&gt;&amp;nbsp;&amp;nbsp;ИМ выключена, указатель захвачен&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;&lt;img src=:/mouse_can_seamless_uncaptured_16px.png/&gt;&amp;nbsp;&amp;nbsp;ИМ выключена, указатель не захвачен&lt;/nobr&gt;&lt;br&gt;Обратите внимание, что для интеграции мыши требуется установка Дополнений гостевой ОС.</translation>
    </message>
    <message>
        <source>Indicates whether the keyboard is captured by the guest OS (&lt;img src=:/hostkey_captured_16px.png/&gt;) or not (&lt;img src=:/hostkey_16px.png/&gt;).</source>
        <translation>Показывает, захвачена клавиатура в гостевой ОС (&lt;img src=:/hostkey_captured_16px.png/&gt;) или нет (&lt;img src=:/hostkey_16px.png/&gt;).</translation>
    </message>
    <message>
        <source>Indicates whether the Remote Desktop Server is enabled (&lt;img src=:/vrdp_16px.png/&gt;) or not (&lt;img src=:/vrdp_disabled_16px.png/&gt;).</source>
        <translation>Показывает, включен удаленный дисплей (VRDP-сервер) (&lt;img src=:/vrdp_16px.png/&gt;) или нет (&lt;img src=:/vrdp_disabled_16px.png/&gt;).</translation>
    </message>
    <message>
        <source>&lt;hr&gt;The Remote Desktop Server is listening on port %1</source>
        <translation>&lt;hr&gt;VRDP-сервер ожидает соединений на порту %1</translation>
    </message>
    <message>
        <source>Indicates the status of different features used by this virtual machine:&lt;br&gt;&lt;nobr&gt;&lt;b&gt;%1:&lt;/b&gt;&amp;nbsp;%2&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;&lt;b&gt;%3:&lt;/b&gt;&amp;nbsp;%4&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;&lt;b&gt;%5:&lt;/b&gt;&amp;nbsp;%6%&lt;/nobr&gt;</source>
        <comment>Virtualization Stuff LED</comment>
        <translation>Отображает статус разнообразных опций, используемых виртуальной машиной:&lt;br&gt;&lt;nobr&gt;&lt;b&gt;%1:&lt;/b&gt;&amp;nbsp;%2&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;&lt;b&gt;%3:&lt;/b&gt;&amp;nbsp;%4&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;&lt;b&gt;%5:&lt;/b&gt;&amp;nbsp;%6%&lt;/nobr&gt;</translation>
    </message>
</context>
<context>
    <name>UIMachineLogic</name>
    <message>
        <source>VirtualBox OSE</source>
        <translation type="obsolete">VirtualBox OSE</translation>
    </message>
    <message>
        <source> EXPERIMENTAL build %1r%2 - %3</source>
        <translation>ЭКСПЕРИМЕНТАЛЬНАЯ версия %1р%2 - %3</translation>
    </message>
    <message>
        <source>Preview Monitor %1</source>
        <translation>Предпросмотр монитора %1</translation>
    </message>
    <message>
        <source>Snapshot %1</source>
        <translation>Снимок %1</translation>
    </message>
    <message>
        <source>More CD/DVD Images...</source>
        <translation type="obsolete">Прочие образы оптических дисков...</translation>
    </message>
    <message>
        <source>Unmount CD/DVD Device</source>
        <translation type="obsolete">Извлечь образ оптического диска</translation>
    </message>
    <message>
        <source>More Floppy Images...</source>
        <translation type="obsolete">Прочие образы гибких дисков...</translation>
    </message>
    <message>
        <source>Unmount Floppy Device</source>
        <translation type="obsolete">Извлечь образ гибкого диска</translation>
    </message>
    <message>
        <source>No CD/DVD Devices Attached</source>
        <translation>Нет подсоединенных приводов оптических дисков</translation>
    </message>
    <message>
        <source>No CD/DVD devices attached to that VM</source>
        <translation>Нет подсоединенных приводов оптических дисков</translation>
    </message>
    <message>
        <source>No Floppy Devices Attached</source>
        <translation>Нет подсоединенных приводов гибких дисков</translation>
    </message>
    <message>
        <source>No floppy devices attached to that VM</source>
        <translation>Нет подсоединенных приводов гибких дисков</translation>
    </message>
    <message>
        <source>No USB Devices Connected</source>
        <translation>Нет подсоединенных USB устройств</translation>
    </message>
    <message>
        <source>No supported devices connected to the host PC</source>
        <translation></translation>
    </message>
</context>
<context>
    <name>UIMachineSettingsAudio</name>
    <message>
        <source>When checked, a virtual PCI audio card will be plugged into the virtual machine and will communicate with the host audio system using the specified driver.</source>
        <translation>Когда стоит галочка, к виртуальной машине подключается виртуальная звуковая PCI-карта, которая использует указанный аудиодрайвер для связи со звуковой картой основного компьютера.</translation>
    </message>
    <message>
        <source>Enable &amp;Audio</source>
        <translation>&amp;Включить аудио</translation>
    </message>
    <message>
        <source>Host Audio &amp;Driver:</source>
        <translation>А&amp;удиодрайвер:</translation>
    </message>
    <message>
        <source>Controls the audio output driver. The &lt;b&gt;Null Audio Driver&lt;/b&gt; makes the guest see an audio card, however every access to it will be ignored.</source>
        <translation>Управляет драйвером основного ПК, используемым для вывода звука. Пункт &lt;b&gt;Пустой аудиодрайвер&lt;/b&gt; позволяет гостевой ОС обнаружить звуковую карту, однако любой доступ к ней будет проигнорирован.</translation>
    </message>
    <message>
        <source>Audio &amp;Controller:</source>
        <translation>Ау&amp;дио-контроллер:</translation>
    </message>
    <message>
        <source>Selects the type of the virtual sound card. Depending on this value, VirtualBox will provide different audio hardware to the virtual machine.</source>
        <translation>Задает тип виртуальной звуковой карты. В зависимости от выбранного значения, VirtualBox обеспечит виртуальную машину соответствующим звуковым устройством.</translation>
    </message>
</context>
<context>
    <name>UIMachineSettingsDisplay</name>
    <message>
        <source>you have assigned less than &lt;b&gt;%1&lt;/b&gt; of video memory which is the minimum amount required to switch the virtual machine to fullscreen or seamless mode.</source>
        <translation>под видеопамять выделено менее &lt;b&gt;%1&lt;/b&gt;. Данное значение является минимальным количеством, необходимым для переключения виртуальной машины в полноэкранный режим или в режим интеграции дисплея.</translation>
    </message>
    <message>
        <source>&lt;qt&gt;%1&amp;nbsp;MB&lt;/qt&gt;</source>
        <translation>&lt;qt&gt;%1&amp;nbsp;МБ&lt;/qt&gt;</translation>
    </message>
    <message>
        <source>&amp;Video</source>
        <translation>&amp;Видео</translation>
    </message>
    <message>
        <source>Video &amp;Memory:</source>
        <translation>Видео &amp;память:</translation>
    </message>
    <message>
        <source>Controls the amount of video memory provided to the virtual machine.</source>
        <translation>Регулирует количество видеопамяти, доступной для виртуальной машины.</translation>
    </message>
    <message>
        <source>MB</source>
        <translation>МБ</translation>
    </message>
    <message>
        <source>Extended Features:</source>
        <translation>Дополнительные возможности:</translation>
    </message>
    <message>
        <source>When checked, the virtual machine will be given access to the 3D graphics capabilities available on the host.</source>
        <translation>Если стоит галочка, виртуальная машина получит доступ к возможностям 3D-графики, имеющимся на основном ПК.</translation>
    </message>
    <message>
        <source>Enable &amp;3D Acceleration</source>
        <translation>В&amp;ключить 3D-ускорение</translation>
    </message>
    <message>
        <source>&amp;Remote Display</source>
        <translation>&amp;Удаленный дисплей</translation>
    </message>
    <message>
        <source>When checked, the VM will act as a Remote Desktop Protocol (RDP) server, allowing remote clients to connect and operate the VM (when it is running) using a standard RDP client.</source>
        <translation>Если стоит галочка, то виртуальная машина будет работать как сервер удаленного рабочего стола (RDP), позволяя удаленным клиентам соединяться и использовать ВМ (когда она работает) с помощью стандартного RDP-клиента.</translation>
    </message>
    <message>
        <source>&amp;Enable Server</source>
        <translation>В&amp;ключить сервер</translation>
    </message>
    <message>
        <source>Server &amp;Port:</source>
        <translation>&amp;Порт сервера:</translation>
    </message>
    <message>
        <source>Displays the VRDP Server port number. You may specify &lt;tt&gt;0&lt;/tt&gt; (zero) to reset the port to the default value.</source>
        <translation type="obsolete">Показывает номер порта VRDP-сервера. Вы можете указать &lt;tt&gt;0&lt;/tt&gt; (ноль) для сброса номера порта к значению по умолчанию.</translation>
    </message>
    <message>
        <source>Authentication &amp;Method:</source>
        <translation>&amp;Метод аутентификации:</translation>
    </message>
    <message>
        <source>Defines the VRDP authentication method.</source>
        <translation>Задает способ авторизации VRDP-сервера.</translation>
    </message>
    <message>
        <source>Authentication &amp;Timeout:</source>
        <translation>В&amp;ремя ожидания аутентификации:</translation>
    </message>
    <message>
        <source>Specifies the timeout for guest authentication, in milliseconds.</source>
        <translation>Задает максимальное время ожидания авторизации подключения к гостевой ОС в миллисекундах.</translation>
    </message>
    <message>
        <source>you have assigned less than &lt;b&gt;%1&lt;/b&gt; of video memory which is the minimum amount required for HD Video to be played efficiently.</source>
        <translation>под видеопамять выделено менее &lt;b&gt;%1&lt;/b&gt;. Данное значение является минимальным количеством, необходимым для того, что бы видео формата HD корректно воспроизводилось.</translation>
    </message>
    <message>
        <source>When checked, the virtual machine will be given access to the Video Acceleration capabilities available on the host.</source>
        <translation>Если стоит галочка, виртуальной машине будет предоставлен доступ к средствам ускорения видео, имеющимся на основном ПК.</translation>
    </message>
    <message>
        <source>Enable &amp;2D Video Acceleration</source>
        <translation>Включить &amp;2D-ускорение видео</translation>
    </message>
    <message>
        <source>The VRDP Server port number. You may specify &lt;tt&gt;0&lt;/tt&gt; (zero), to select port 3389, the standard port for RDP.</source>
        <translation>Показывает номер порта VRDP-сервера. Вы можете указать &lt;tt&gt;0&lt;/tt&gt; (ноль) для сброса номера порта к значению по умолчанию для RDP - 3389.</translation>
    </message>
    <message>
        <source>Mo&amp;nitor Count:</source>
        <translation>Количество &amp;мониторов:</translation>
    </message>
    <message>
        <source>Controls the amount of virtual monitors provided to the virtual machine.</source>
        <translation>Задаёт количество виртуальных мониторов, подключенных к данной виртуальной машине.</translation>
    </message>
    <message>
        <source>&lt;qt&gt;%1&lt;/qt&gt;</source>
        <translation>&lt;qt&gt;%1&lt;/qt&gt;</translation>
    </message>
    <message>
        <source>Specifies whether multiple simultaneous connections to the VM are permitted.</source>
        <translation>Определяет, разрешено ли несколько одновременных подключений к ВМ.</translation>
    </message>
    <message>
        <source>&amp;Allow Multiple Connections</source>
        <translation>&amp;Разрешать несколько подключений</translation>
    </message>
    <message>
        <source>You have 3D Acceleration enabled for a operation system which uses the WDDM video driver. For maximal performance set the guest VRAM to at least &lt;b&gt;%1&lt;/b&gt;.</source>
        <translation type="obsolete">Вы решили использовать 3D-ускорение для гостевой ОС, использующей WDDM видео-драйвер. Для достижения максимальной производительности задайте количество видео-памяти гостевой ОС минимум в &lt;b&gt;%1&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>you have 3D Acceleration enabled for a operation system which uses the WDDM video driver. For maximal performance set the guest VRAM to at least &lt;b&gt;%1&lt;/b&gt;.</source>
        <translation>Вы решили использовать 3D-ускорение для гостевой ОС, использующей WDDM видео-драйвер. Для достижения максимальной производительности задайте количество видео-памяти гостевой ОС минимум в &lt;b&gt;%1&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>you have 2D Video Acceleration enabled. As 2D Video Acceleration is supported for Windows guests only, this feature will be disabled.</source>
        <translation>для этой машины выбрана функция 2D-ускорения видео. Поскольку данная функция поддерживается лишь классом гостевых систем Windows, она будет отключена.</translation>
    </message>
</context>
<context>
    <name>UIMachineSettingsGeneral</name>
    <message>
        <source>&lt;qt&gt;%1&amp;nbsp;MB&lt;/qt&gt;</source>
        <translation type="obsolete">&lt;qt&gt;%1&amp;nbsp;МБ&lt;/qt&gt;</translation>
    </message>
    <message>
        <source>Displays the path where snapshots of this virtual machine will be stored. Be aware that snapshots can take quite a lot of disk space.</source>
        <translation>Показывает путь к папке для сохранения снимков этой виртуальной машины. Имейте ввиду, что снимки могут занимать достаточно много места на жестком диске.</translation>
    </message>
    <message>
        <source>&amp;Basic</source>
        <translation>О&amp;сновные</translation>
    </message>
    <message>
        <source>Identification</source>
        <translation type="obsolete">Идентификация</translation>
    </message>
    <message>
        <source>&amp;Name:</source>
        <translation>&amp;Имя:</translation>
    </message>
    <message>
        <source>Displays the name of the virtual machine.</source>
        <translation>Указывает имя виртуальной машины.</translation>
    </message>
    <message>
        <source>Base &amp;Memory Size</source>
        <translation type="obsolete">Ра&amp;змер основной памяти</translation>
    </message>
    <message>
        <source>Controls the amount of memory provided to the virtual machine. If you assign too much, the machine might not start.</source>
        <translation type="obsolete">Регулирует количество памяти, доступной для виртуальной машины. Если установить слишком большое значение, то машина может не запуститься.</translation>
    </message>
    <message>
        <source>&lt;</source>
        <translation type="obsolete">&lt;</translation>
    </message>
    <message>
        <source>&gt;</source>
        <translation type="obsolete">&gt;</translation>
    </message>
    <message>
        <source>MB</source>
        <translation type="obsolete">МБ</translation>
    </message>
    <message>
        <source>&amp;Video Memory Size</source>
        <translation type="obsolete">Раз&amp;мер видеопамяти</translation>
    </message>
    <message>
        <source>Controls the amount of video memory provided to the virtual machine.</source>
        <translation type="obsolete">Регулирует количество видеопамяти, доступной для виртуальной машины.</translation>
    </message>
    <message>
        <source>&amp;Advanced</source>
        <translation>&amp;Дополнительно</translation>
    </message>
    <message>
        <source>Boo&amp;t Order:</source>
        <translation type="obsolete">Пор&amp;ядок загрузки:</translation>
    </message>
    <message>
        <source>Defines the boot device order. Use the checkboxes on the left to enable or disable individual boot devices. Move items up and down to change the device order.</source>
        <translation type="obsolete">Определяет порядок загрузочных устройств. Используйте галочки слева, чтобы разрешить или запретить загрузку с отдельных устройств. Порядок устройств изменяется перемещением их вверх и вниз.</translation>
    </message>
    <message>
        <source>Move Up (Ctrl-Up)</source>
        <translation type="obsolete">Вверх (Ctrl-Up)</translation>
    </message>
    <message>
        <source>Moves the selected boot device up.</source>
        <translation type="obsolete">Перемещает выбранное загрузочное устройство вверх.</translation>
    </message>
    <message>
        <source>Move Down (Ctrl-Down)</source>
        <translation type="obsolete">Вниз (Ctrl-Down)</translation>
    </message>
    <message>
        <source>Moves the selected boot device down.</source>
        <translation type="obsolete">Перемещает выбранное загрузочное устройство вниз.</translation>
    </message>
    <message>
        <source>Extended Features:</source>
        <translation type="obsolete">Дополнительные возможности:</translation>
    </message>
    <message>
        <source>When checked, the virtual machine will support the Advanced Configuration and Power Management Interface (ACPI). &lt;b&gt;Note:&lt;/b&gt; don&apos;t disable this feature after having installed a Windows guest operating system!</source>
        <translation type="obsolete">&lt;qt&gt;Если стоит галочка, то виртуальная машина будет поддерживать улучшенный интерфейс для конфигурации и управления электропитанием (ACPI). &lt;b&gt;Примечание:&lt;/b&gt; невыключайте это свойство после установки Windows в качестве гостевой ОС!&lt;/qt&gt;</translation>
    </message>
    <message>
        <source>Enable A&amp;CPI</source>
        <translation type="obsolete">&amp;Включить ACPI</translation>
    </message>
    <message>
        <source>When checked, the virtual machine will support the Input Output APIC (IO APIC), which may slightly decrease performance. &lt;b&gt;Note:&lt;/b&gt; don&apos;t disable this feature after having installed a Windows guest operating system!</source>
        <translation type="obsolete">&lt;qt&gt;Если стоит галочка, то виртуальная машина будет поддерживать операции ввода/вывода контроллера прерываний (IO APIC), что может слегка снизить производительность ВМ. &lt;b&gt;Примечание:&lt;/b&gt; не выключайте это свойство после установки Windows в качестве гостевой ОС!&lt;/qt&gt;</translation>
    </message>
    <message>
        <source>Enable IO A&amp;PIC</source>
        <translation type="obsolete">В&amp;ключить IO APIC</translation>
    </message>
    <message>
        <source>When checked, the virtual machine will try to make use of the host CPU&apos;s hardware virtualization extensions such as Intel VT-x and AMD-V.</source>
        <translation type="obsolete">Если стоит галочка, виртуальная машина будет пытаться задействовать расширенные функции аппаратной виртуализации процессора основного ПК, такие как Intel VT-x или AMD-V.</translation>
    </message>
    <message>
        <source>Enable &amp;VT-x/AMD-V</source>
        <translation type="obsolete">Вк&amp;лючить VT-x/AMD-V</translation>
    </message>
    <message>
        <source>When checked, the Physical Address Extension (PAE) feature of the host CPU will be exposed to the virtual machine.</source>
        <translation type="obsolete">Если стоит галочка, виртуальной машине будет предоставлен доступ к функции Physical Address Extension (PAE, расширение физического адреса) центрального процессора основного ПК.</translation>
    </message>
    <message>
        <source>Enable PA&amp;E/NX</source>
        <translation type="obsolete">Вклю&amp;чить PAE/NX</translation>
    </message>
    <message>
        <source>&amp;Shared Clipboard:</source>
        <translation>О&amp;бщий буфер обмена:</translation>
    </message>
    <message>
        <source>Selects which clipboard data will be copied between the guest and the host OS. This feature requires Guest Additions to be installed in the guest OS.</source>
        <translation>Задает режим работы буфера обмена между гостевой и основной ОС. Заметьте, что использование этой функции требует установки пакета Дополнений гостевой ОС.</translation>
    </message>
    <message>
        <source>Defines the type of the virtual IDE controller. Depending on this value, VirtualBox will provide different virtual IDE hardware devices to the guest OS.</source>
        <translation type="obsolete">Задает тип виртуального контроллера IDE. В зависимости от выбранного значения, VirtualBox обеспечит виртуальную машину соответствующим IDE-устройством.</translation>
    </message>
    <message>
        <source>&amp;IDE Controller Type:</source>
        <translation type="obsolete">&amp;Тип контроллера IDE:</translation>
    </message>
    <message>
        <source>S&amp;napshot Folder:</source>
        <translation>Папка для с&amp;нимков:</translation>
    </message>
    <message>
        <source>&amp;Description</source>
        <translation>О&amp;писание</translation>
    </message>
    <message>
        <source>Displays the description of the virtual machine. The description field is useful for commenting on configuration details of the installed guest OS.</source>
        <translation>Показывает описание виртуальной машины. Поле описания удобно использовать для занесения заметок о настройках установленной гостевой ОС.</translation>
    </message>
    <message>
        <source>&amp;Other</source>
        <translation type="obsolete">П&amp;рочие</translation>
    </message>
    <message>
        <source>If checked, any change to mounted CD/DVD or Floppy media performed during machine execution will be saved in the settings file in order to preserve the configuration of mounted media between runs.</source>
        <translation>Если стоит галочка, то любое изменение подключенных CD/DVD-носителей или гибких дисков, произведенное во время работы машины, будет сохранено в файле настроек для восстановления конфигурации подключенных носителей при последующих запусках.</translation>
    </message>
    <message>
        <source>&amp;Remember Mounted Media</source>
        <translation type="obsolete">&amp;Запоминать подключенные носители</translation>
    </message>
    <message>
        <source>Runtime:</source>
        <translation type="obsolete">Работа:</translation>
    </message>
    <message>
        <source>you have assigned more than &lt;b&gt;75%&lt;/b&gt; of your computer&apos;s memory (&lt;b&gt;%1&lt;/b&gt;) to the virtual machine. Not enough memory is left for your host operating system. Please select a smaller amount.</source>
        <translation type="obsolete">виртуальной машине назначено более &lt;b&gt;75%&lt;/b&gt; памяти компьютера (&lt;b&gt;%1&lt;/b&gt;). Недостаточно памяти для операционной системы основного ПК. Задайте меньшее значение.</translation>
    </message>
    <message>
        <source>you have assigned more than &lt;b&gt;50%&lt;/b&gt; of your computer&apos;s memory (&lt;b&gt;%1&lt;/b&gt;) to the virtual machine. There might not be enough memory left for your host operating system. Continue at your own risk.</source>
        <translation type="obsolete">виртуальной машине назначено более &lt;b&gt;50%&lt;/b&gt; памяти компьютера (&lt;b&gt;%1&lt;/b&gt;). Для операционной системы основного ПК может оказаться недостаточно памяти. Продолжайте только на свой страх и риск.</translation>
    </message>
    <message>
        <source>you have assigned less than &lt;b&gt;%1&lt;/b&gt; of video memory which is the minimum amount required to switch the virtual machine to fullscreen or seamless mode.</source>
        <translation type="obsolete">под видеопамять выделено менее &lt;b&gt;%1&lt;/b&gt;. Данное значение является минимальным количеством, необходимым для переключения виртуальной машины в полноэкранный режим или в режим интеграции дисплея.</translation>
    </message>
    <message>
        <source>When checked, the virtual machine will be given access to the 3D graphics capabilities available on the host.</source>
        <translation type="obsolete">Если стоит галочка, виртуальная машина получит доступ к возможностям 3D-графики, имеющимся на основном ПК.</translation>
    </message>
    <message>
        <source>Enable &amp;3D Acceleration</source>
        <translation type="obsolete">Включить &amp;3D-ускорение</translation>
    </message>
    <message>
        <source>you have assigned more than &lt;b&gt;%1%&lt;/b&gt; of your computer&apos;s memory (&lt;b&gt;%2&lt;/b&gt;) to the virtual machine. Not enough memory is left for your host operating system. Please select a smaller amount.</source>
        <translation type="obsolete">виртуальной машине назначено более &lt;b&gt;%1%&lt;/b&gt; памяти компьютера (&lt;b&gt;%2&lt;/b&gt;). Недостаточно памяти для операционной системы основного ПК. Задайте меньшее значение.</translation>
    </message>
    <message>
        <source>you have assigned more than &lt;b&gt;%1%&lt;/b&gt; of your computer&apos;s memory (&lt;b&gt;%2&lt;/b&gt;) to the virtual machine. There might not be enough memory left for your host operating system. Continue at your own risk.</source>
        <translation type="obsolete">виртуальной машине назначено более &lt;b&gt;%1%&lt;/b&gt; памяти компьютера (&lt;b&gt;%2&lt;/b&gt;). Для операционной системы основного ПК может оказаться недостаточно памяти. Продолжайте только на свой страх и риск.</translation>
    </message>
    <message>
        <source>there is a 64 bits guest OS type assigned for this VM, which requires virtualization feature (VT-x/AMD-V) to be enabled too, else your guest will fail to detect a 64 bits CPU and will not be able to boot, so this feature will be enabled automatically when you&apos;ll accept VM Settings by pressing OK button.</source>
        <translation type="obsolete">для данной машины выбран 64х-битный тип ОС, требующий активации функции аппаратной виртуализации (VT-x/AMD-V), иначе гость не сможет определить 64х-битный процессор и загрузиться, поэтому эта функция будет автоматически включена в момент сохранения настроек ВМ.</translation>
    </message>
    <message>
        <source>When checked, the virtual machine will try to make use of the nested paging extension of Intel VT-x and AMD-V.</source>
        <translation type="obsolete">Если стоит галочка, виртуальная машина будет пытаться использовать расширение Nested Paging для функций аппаратной виртуализации Intel VT-x and AMD-V.</translation>
    </message>
    <message>
        <source>Enable Nested Pa&amp;ging</source>
        <translation type="obsolete">Вкл&amp;ючить  Nested Paging</translation>
    </message>
    <message>
        <source>Removable Media:</source>
        <translation>Сменные носители информации:</translation>
    </message>
    <message>
        <source>&amp;Remember Runtime Changes</source>
        <translation>&amp;Запоминать изменения в процессе работы ВМ</translation>
    </message>
    <message>
        <source>Mini ToolBar:</source>
        <translation>Мини тулбар:</translation>
    </message>
    <message>
        <source>If checked, show the Mini ToolBar in Fullscreen and Seamless modes.</source>
        <translation>Если стоит галочка, в полноэкранных режимах работы будет использоваться мини тулбар.</translation>
    </message>
    <message>
        <source>Show In &amp;Fullscreen/Seamless</source>
        <translation>&amp;Использовать в полноэкранных режимах</translation>
    </message>
    <message>
        <source>If checked, show the Mini ToolBar at the top of the screen, rather than in its default position at the bottom of the screen.</source>
        <translation>Если стоит галочка, мини тулбар будет расположен сверху, в отличие от своей позиции по-умолчанию - снизу.</translation>
    </message>
    <message>
        <source>Show At &amp;Top Of Screen</source>
        <translation>&amp;Расположить сверху экрана</translation>
    </message>
    <message>
        <source>you have selected a 64-bit guest OS type for this VM. As such guests require hardware virtualization (VT-x/AMD-V), this feature will be enabled automatically.</source>
        <translation>для этой машины выбран 64-битный тип гостевой ОС. В связи с тем, что такие гостевые ОС требуют активации функций аппаратной виртуализации (VT-x/AMD-V), эти функции будут включены автоматически.</translation>
    </message>
</context>
<context>
    <name>UIMachineSettingsNetwork</name>
    <message>
        <source>Select TAP setup application</source>
        <translation type="obsolete">Выберите программу настройки TAP-интерфейса</translation>
    </message>
    <message>
        <source>Select TAP terminate application</source>
        <translation type="obsolete">Выберите программу удаления TAP-интерфейса</translation>
    </message>
    <message>
        <source>When checked, plugs this virtual network adapter into the virtual machine.</source>
        <translation>Если стоит галочка, то этот виртуальный сетевой адаптер будет подключен к виртуальной машине.</translation>
    </message>
    <message>
        <source>&amp;Enable Network Adapter</source>
        <translation>&amp;Включить сетевой адаптер</translation>
    </message>
    <message>
        <source>A&amp;dapter Type:</source>
        <translation type="obsolete">&amp;Тип адаптера:</translation>
    </message>
    <message>
        <source>Selects the type of the virtual network adapter. Depending on this value, VirtualBox will provide different network hardware to the virtual machine.</source>
        <translation>Задает тип виртуального сетевого адаптера. В зависимости от выбранного значения, VirtualBox обеспечит виртуальную машину соответствующим сетевым устройством.</translation>
    </message>
    <message>
        <source>&amp;Attached to:</source>
        <translation>Тип &amp;подключения:</translation>
    </message>
    <message>
        <source>Controls how this virtual adapter is attached to the real network of the Host OS.</source>
        <translation>Определяет способ, которым этот виртуальный сетевой адаптер подсоединяется к настоящей сети основной ОС.</translation>
    </message>
    <message>
        <source>&amp;Network Name:</source>
        <translation type="obsolete">&amp;Имя сети:</translation>
    </message>
    <message>
        <source>Displays the name of the internal network selected for this adapter.</source>
        <translation type="obsolete">Показывает имя внутренней сети, выбранной для этого адаптера.</translation>
    </message>
    <message>
        <source>&amp;MAC Address:</source>
        <translation type="obsolete">MAC-а&amp;дрес:</translation>
    </message>
    <message>
        <source>Displays the MAC address of this adapter. It contains exactly 12 characters chosen from {0-9,A-F}. Note that the second character must be an even digit.</source>
        <translation>Показывает MAC-адрес этого адаптера. Он состоит ровно из 12 символов из диапазона {0-9,A-F}. Имейте ввиду, что второй символ должен быть четной цифрой.</translation>
    </message>
    <message>
        <source>Generates a new random MAC address.</source>
        <translation>Генерирует новый случайный MAC-адрес.</translation>
    </message>
    <message>
        <source>&amp;Generate</source>
        <translation type="obsolete">С&amp;генерировать</translation>
    </message>
    <message>
        <source>Indicates whether the virtual network cable is plugged in on machine startup or not.</source>
        <translation>Определяет, подключен виртуальный сетевой кабель при запуске машины или нет.</translation>
    </message>
    <message>
        <source>Ca&amp;ble Connected</source>
        <translation type="obsolete">&amp;Кабель подключен</translation>
    </message>
    <message>
        <source>&amp;Interface Name:</source>
        <translation type="obsolete">И&amp;мя интерфейса:</translation>
    </message>
    <message>
        <source>Displays the TAP interface name.</source>
        <translation type="obsolete">Показывает имя TAP-интерфейса.</translation>
    </message>
    <message>
        <source>&amp;Setup Application:</source>
        <translation type="obsolete">П&amp;рограмма настройки:</translation>
    </message>
    <message>
        <source>Displays the command executed to set up the TAP interface.</source>
        <translation type="obsolete">Показывает команду (приложение или скрипт), выполяемую для создания и настройки TAP-интерфейса.</translation>
    </message>
    <message>
        <source>Selects the setup application.</source>
        <translation type="obsolete">Выбирает программу для настройки.</translation>
    </message>
    <message>
        <source>&amp;Terminate Application:</source>
        <translation type="obsolete">Программа &amp;удаления:</translation>
    </message>
    <message>
        <source>Displays the command executed to terminate the TAP interface.</source>
        <translation type="obsolete">Показывает команду (приложение или скрипт), выполняемую для удаления TAP-интерфейса.</translation>
    </message>
    <message>
        <source>Selects the terminate application.</source>
        <translation type="obsolete">Выбирает программу для удаления.</translation>
    </message>
    <message>
        <source>Host Interface Settings</source>
        <translation type="obsolete">Настройки хост-интерфейса </translation>
    </message>
    <message>
        <source>Adapter</source>
        <comment>network</comment>
        <translation type="obsolete">Адаптер</translation>
    </message>
    <message>
        <source>Not selected</source>
        <comment>adapter</comment>
        <translation type="obsolete">Не выбран</translation>
    </message>
    <message>
        <source>Network</source>
        <comment>internal</comment>
        <translation type="obsolete">Сеть</translation>
    </message>
    <message>
        <source>Not selected</source>
        <comment>network</comment>
        <translation type="obsolete">Не выбрана</translation>
    </message>
    <message>
        <source>MAC Address</source>
        <translation type="obsolete">MAC-адрес</translation>
    </message>
    <message>
        <source>Not selected</source>
        <comment>address</comment>
        <translation type="obsolete">Не указан</translation>
    </message>
    <message>
        <source>Cable</source>
        <translation type="obsolete">Кабель</translation>
    </message>
    <message>
        <source>Connected</source>
        <comment>cable</comment>
        <translation type="obsolete">Подсоединён</translation>
    </message>
    <message>
        <source>Not connected</source>
        <comment>cable</comment>
        <translation type="obsolete">Не подсоединён</translation>
    </message>
    <message>
        <source>Adapter &amp;Type:</source>
        <translation>&amp;Тип адаптера:</translation>
    </message>
    <message>
        <source>Open extended settings dialog for current attachment type.</source>
        <translation type="obsolete">Открыть диалог расширенных настроек для выбранного типа подключения.</translation>
    </message>
    <message>
        <source>no bridged network adapter is selected</source>
        <translation>не выбран адаптер для подключения по сетевому мосту.</translation>
    </message>
    <message>
        <source>no internal network name is specified</source>
        <translation>не указано имя внутренней сети.</translation>
    </message>
    <message>
        <source>no host-only network adapter is selected</source>
        <translation>не выбран виртуальный сетевой адаптер хоста.</translation>
    </message>
    <message>
        <source>Not selected</source>
        <comment>network adapter name</comment>
        <translation>Не выбрано</translation>
    </message>
    <message>
        <source>Open additional options dialog for current attachment type.</source>
        <translation type="obsolete">Открыть диалог дополнительных опций данного типа подключения.</translation>
    </message>
    <message>
        <source>&amp;Name:</source>
        <translation>&amp;Имя:</translation>
    </message>
    <message>
        <source>Selects the name of the network adapter for &lt;b&gt;Bridged Adapter&lt;/b&gt; or &lt;b&gt;Host-only Adapter&lt;/b&gt; attachments and the name of the network &lt;b&gt;Internal Network&lt;/b&gt; attachments.</source>
        <translation type="obsolete">Позволяет выбрать имя сетевого адаптера, если тип подключения - &lt;b&gt;Сетевой мост&lt;/b&gt; или &lt;b&gt;Виртуальный адаптер хоста&lt;/b&gt;, либо имя внутренней сети, если тип подключения - &lt;b&gt;Внутренняя сеть&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>A&amp;dvanced</source>
        <translation>&amp;Дополнительно</translation>
    </message>
    <message>
        <source>Shows or hides additional network adapter options.</source>
        <translation>Показывает/скрывает дополнительные опции сетевого адаптера.</translation>
    </message>
    <message>
        <source>&amp;Mac Address:</source>
        <translation>MAC-&amp;адрес:</translation>
    </message>
    <message>
        <source>&amp;Cable connected</source>
        <translation>&amp;Кабель подключен</translation>
    </message>
    <message>
        <source>Opens dialog to manage port forwarding rules.</source>
        <translation>Открыть диалог для управления правилами проброса портов.</translation>
    </message>
    <message>
        <source>&amp;Port Forwarding</source>
        <translation>&amp;Проброс портов</translation>
    </message>
    <message>
        <source>&amp;Promiscuous Mode:</source>
        <translation>&amp;Неразборчивый режим:</translation>
    </message>
    <message>
        <source>Selects the promiscuous mode policy of the network adapter when attached to an internal network, host only network or a bridge.</source>
        <translation>Задаёт политику &quot;неразборчивого&quot; режима данного виртуального сетевого адаптера, если он подключен к внутренней сети, виртуальному сетевому адаптеру хоста или сетевому мосту.</translation>
    </message>
    <message>
        <source>Generic Properties:</source>
        <translation>Параметры драйвера:</translation>
    </message>
    <message>
        <source>Enter any configuration settings here for the network attachment driver you will be using. The settings should be of the form &lt;b&gt;name=value&lt;/b&gt; and will depend on the driver. Use &lt;b&gt;shift-enter&lt;/b&gt; to add a new entry.</source>
        <translation>Здесь Вы можете задать необходимые параметры сетевого драйвера. Параметры должны быть заданы в форме &lt;b&gt;имя=значение&lt;/b&gt; и зависят от самого драйвера. Используйте &lt;b&gt;shift-enter&lt;/b&gt; для перехода на новую строку.</translation>
    </message>
    <message>
        <source>no generic driver is selected</source>
        <translation>не выбран универсальный драйвер.</translation>
    </message>
    <message>
        <source>Selects the network adapter on the host system that traffic to and from this network card will go through.</source>
        <translation>Задаёт сетевой адаптер хоста, через который пойдёт трафик данного виртуального сетевого адаптера.</translation>
    </message>
    <message>
        <source>Enter the name of the internal network that this network card will be connected to. You can create a new internal network by choosing a name which is not used by any other network cards in this virtual machine or others.</source>
        <translation>Задаёт имя внутренней сети, к которой будет подключен данный виртуальный сетевой адаптер. Вы можете создать новую внутреннюю сеть, выбрав имя, которое не используется иными виртуальными сетевыми адаптерами данной и других машин.</translation>
    </message>
    <message>
        <source>Selects the virtual network adapter on the host system that traffic to and from this network card will go through. You can create and remove adapters using the global network settings in the virtual machine manager window.</source>
        <translation>Задаёт виртуальный сетевой адаптер хоста, через который пойдёт трафик данного виртуального сетевого адаптера. Вы можете добавлять и удалять виртуальные сетевые адаптеры хоста, используя окно глобальных свойств менеджера виртуальных машин.</translation>
    </message>
    <message>
        <source>Selects the driver to be used with this network card.</source>
        <translation>Задаёт имя универсального сетевого драйвера, который будет использоваться для данного виртуального сетевого адаптера.</translation>
    </message>
</context>
<context>
    <name>UIMachineSettingsNetworkDetails</name>
    <message>
        <source>no bridged network adapter is selected</source>
        <translation type="obsolete">не выбран адаптер для подключения по сетевому мосту</translation>
    </message>
    <message>
        <source>no internal network name is specified</source>
        <translation type="obsolete">не указано имя внутренней сети</translation>
    </message>
    <message>
        <source>no host-only adapter is selected</source>
        <translation type="obsolete">не выбран виртуальный сетевой адаптер хоста</translation>
    </message>
    <message>
        <source>Basic Details</source>
        <translation type="obsolete">Базовые атрибуты</translation>
    </message>
    <message>
        <source>Bridged Network Details</source>
        <translation type="obsolete">Атрибуты сетевого моста</translation>
    </message>
    <message>
        <source>Internal Network Details</source>
        <translation type="obsolete">Атрибуты внутренней сети</translation>
    </message>
    <message>
        <source>Host-only Network Details</source>
        <translation type="obsolete">Атрибуты виртуальной сети хоста</translation>
    </message>
    <message>
        <source>Not selected</source>
        <translation type="obsolete">Не выбрано</translation>
    </message>
    <message>
        <source>Host Settings</source>
        <translation type="obsolete">Настройки хоста</translation>
    </message>
    <message>
        <source>&amp;Bridged Network Adapter:</source>
        <translation type="obsolete">Адаптер сетевого &amp;моста:</translation>
    </message>
    <message>
        <source>Displays the name of the host network adapter selected for bridged networking.</source>
        <translation type="obsolete">Отображает имя адаптера хоста, используемого для создания сетевого моста.</translation>
    </message>
    <message>
        <source>Internal &amp;Network:</source>
        <translation type="obsolete">&amp;Внутренняя сеть:</translation>
    </message>
    <message>
        <source>Displays the name of the internal network selected for this adapter.</source>
        <translation type="obsolete">Отображает имя внутренней сети для данного виртуального адаптера.</translation>
    </message>
    <message>
        <source>Host-only &amp;Network Adapter:</source>
        <translation type="obsolete">&amp;Виртуальный сетевой адаптер хоста:</translation>
    </message>
    <message>
        <source>Displays the name of the VirtualBox network adapter selected for host-only networking.</source>
        <translation type="obsolete">Отображает имя виртуального адаптера VirtualBox, выбранного для виртуальной сети хоста.</translation>
    </message>
    <message>
        <source>Guest Settings</source>
        <translation type="obsolete">Настройки гостя</translation>
    </message>
    <message>
        <source>Guest &amp;MAC Address:</source>
        <translation type="obsolete">MAC-&amp;адрес гостя:</translation>
    </message>
    <message>
        <source>Displays the MAC address of this adapter. It contains exactly 12 characters chosen from {0-9,A-F}. Note that the second character must be an even digit.</source>
        <translation type="obsolete">Показывает MAC-адрес этого адаптера. Он состоит ровно из 12 символов из диапазона {0-9,A-F}. Имейте ввиду, что второй символ должен быть четной цифрой.</translation>
    </message>
    <message>
        <source>Generates a new random MAC address.</source>
        <translation type="obsolete">Генерирует новый случайный MAC-адрес.</translation>
    </message>
    <message>
        <source>&amp;Cable connected</source>
        <translation type="obsolete">&amp;Кабель подключен</translation>
    </message>
    <message>
        <source>Indicates whether the virtual network cable is plugged in on machine startup or not.</source>
        <translation type="obsolete">Определяет, подключен виртуальный сетевой кабель при запуске машины или нет.</translation>
    </message>
    <message>
        <source>Additional Options</source>
        <translation type="obsolete">Дополнительные опции</translation>
    </message>
</context>
<context>
    <name>UIMachineSettingsNetworkPage</name>
    <message>
        <source>No host network interface is selected</source>
        <translation type="obsolete">Не выбран сетевой хост-интерфейс</translation>
    </message>
    <message>
        <source>Internal network name is not set</source>
        <translation type="obsolete">Не задано имя внутренней сети</translation>
    </message>
</context>
<context>
    <name>UIMachineSettingsParallel</name>
    <message>
        <source>Port %1</source>
        <comment>parallel ports</comment>
        <translation>Порт %1</translation>
    </message>
    <message>
        <source>When checked, enables the given parallel port of the virtual machine.</source>
        <translation>Когда стоит галочка, активизируется указанный параллельный порт виртуальной машины.</translation>
    </message>
    <message>
        <source>&amp;Enable Parallel Port</source>
        <translation>&amp;Включить параллельный порт</translation>
    </message>
    <message>
        <source>Port &amp;Number:</source>
        <translation>&amp;Номер порта:</translation>
    </message>
    <message>
        <source>Displays the parallel port number. You can choose one of the standard parallel ports or select &lt;b&gt;User-defined&lt;/b&gt; and specify port parameters manually.</source>
        <translation>Задает номер параллельного порта. Вы можете выбрать один из стандартных номеров портов или выбрать &lt;b&gt;Пользовательский&lt;/b&gt; и указать параметры порта вручную.</translation>
    </message>
    <message>
        <source>&amp;IRQ:</source>
        <translation>&amp;Прерывание:</translation>
    </message>
    <message>
        <source>Displays the IRQ number of this parallel port. Valid values are integer numbers in range from &lt;tt&gt;0&lt;/tt&gt; to &lt;tt&gt;255&lt;/tt&gt;. Values greater than &lt;tt&gt;15&lt;/tt&gt; may only be used if the &lt;b&gt;IO APIC&lt;/b&gt; is enabled for this virtual machine.</source>
        <translation type="obsolete">Показывает номер прерывания (IRQ) для этого параллельного порта. Допустимые значения -- целые числа в диапазоне от &lt;tt&gt;0&lt;/tt&gt; до &lt;tt&gt;255&lt;/tt&gt;. Значения больше &lt;tt&gt;15&lt;/tt&gt; могут использоваться только в том случае, если для этой машины включен &lt;b&gt;IO APIC&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>I/O Po&amp;rt:</source>
        <translation>Пор&amp;т В/В:</translation>
    </message>
    <message>
        <source>Displays the base I/O port address of this parallel port. This should be a whole number between &lt;tt&gt;0&lt;/tt&gt; and &lt;tt&gt;0xFFFF&lt;/tt&gt;.</source>
        <translation type="obsolete">Показывает базовый адрес ввода-вывода для этого параллельного порта. Допустимые значения -- целые числа в диапазоне от &lt;tt&gt;0&lt;/tt&gt; до &lt;tt&gt;0xFFFF&lt;/tt&gt;.</translation>
    </message>
    <message>
        <source>Port &amp;Path:</source>
        <translation>П&amp;уть к порту:</translation>
    </message>
    <message>
        <source>Displays the host parallel device name.</source>
        <translation>Показывает имя устройства параллельного порта основного ПК.</translation>
    </message>
    <message>
        <source>Displays the IRQ number of this parallel port. This should be a whole number between &lt;tt&gt;0&lt;/tt&gt; and &lt;tt&gt;255&lt;/tt&gt;. Values greater than &lt;tt&gt;15&lt;/tt&gt; may only be used if the &lt;b&gt;IO APIC&lt;/b&gt; setting is enabled for this virtual machine.</source>
        <translation>Показывает номер прерывания (IRQ) для этого параллельного порта. Допустимые значения -- целые числа в диапазоне от &lt;tt&gt;0&lt;/tt&gt; до &lt;tt&gt;255&lt;/tt&gt;. Значения больше &lt;tt&gt;15&lt;/tt&gt; могут использоваться только в том случае, если для этой машины включен &lt;b&gt;IO APIC&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Displays the base I/O port address of this parallel port. Valid values are integer numbers in range from &lt;tt&gt;0&lt;/tt&gt; to &lt;tt&gt;0xFFFF&lt;/tt&gt;.</source>
        <translation>Показывает базовый адрес ввода-вывода для этого параллельного порта. Допустимые значения -- целые числа в диапазоне от &lt;tt&gt;0&lt;/tt&gt; до &lt;tt&gt;0xFFFF&lt;/tt&gt;.</translation>
    </message>
</context>
<context>
    <name>UIMachineSettingsParallelPage</name>
    <message>
        <source>Duplicate port number selected </source>
        <translation>Выбран повторяющийся номер порта</translation>
    </message>
    <message>
        <source>Port path not specified </source>
        <translation>Не задан путь к порту</translation>
    </message>
    <message>
        <source>Duplicate port path entered </source>
        <translation>Введен повторяющийся путь к порту</translation>
    </message>
</context>
<context>
    <name>UIMachineSettingsPortForwardingDlg</name>
    <message>
        <source>Port Forwarding Rules</source>
        <translation>Правила проброса портов</translation>
    </message>
    <message>
        <source>This table contains a list of port forwarding rules.</source>
        <translation>Данная таблица содержит список правил проброса портов.</translation>
    </message>
    <message>
        <source>Insert new rule</source>
        <translation>Добавить новое правило</translation>
    </message>
    <message>
        <source>Copy selected rule</source>
        <translation>Клонировать выбранное правило</translation>
    </message>
    <message>
        <source>Delete selected rule</source>
        <translation>Удалить выбранное правило</translation>
    </message>
    <message>
        <source>This button adds new port forwarding rule.</source>
        <translation>Данная кнопка добавляет новое правило проброса порта.</translation>
    </message>
    <message>
        <source>This button deletes selected port forwarding rule.</source>
        <translation>Данная кнопка удаляет выбранное правило проброса порта.</translation>
    </message>
</context>
<context>
    <name>UIMachineSettingsSF</name>
    <message>
        <source>&amp;Add New Shared Folder</source>
        <translation type="obsolete">&amp;Добавить новую общую папку</translation>
    </message>
    <message>
        <source>&amp;Edit Selected Shared Folder</source>
        <translation type="obsolete">&amp;Изменить выбранную общую папку</translation>
    </message>
    <message>
        <source>&amp;Remove Selected Shared Folder</source>
        <translation type="obsolete">&amp;Удалить выбранную общую папку</translation>
    </message>
    <message>
        <source>Adds a new shared folder definition.</source>
        <translation>Добавляет определение новой общей папки.</translation>
    </message>
    <message>
        <source>Edits the selected shared folder definition.</source>
        <translation>Изменяет определение выбранной общей папки.</translation>
    </message>
    <message>
        <source>Removes the selected shared folder definition.</source>
        <translation>Удаляет определение выбранной общей папки.</translation>
    </message>
    <message>
        <source> Machine Folders</source>
        <translation> Папки машины</translation>
    </message>
    <message>
        <source> Transient Folders</source>
        <translation> Временные папки</translation>
    </message>
    <message>
        <source>Full</source>
        <translation>Полный</translation>
    </message>
    <message>
        <source>Read-only</source>
        <translation>Чтение</translation>
    </message>
    <message>
        <source>Lists all shared folders accessible to this machine. Use &apos;net use x: \\vboxsvr\share&apos; to access a shared folder named &lt;i&gt;share&lt;/i&gt; from a DOS-like OS, or &apos;mount -t vboxsf share mount_point&apos; to access it from a Linux OS. This feature requires Guest Additions.</source>
        <translation>Перечисляет все общие папки, доступные этой машине. Используйте команду &lt;tt&gt;net use x: \\vboxsvr\share&lt;/tt&gt; для доступа к общей папке с именем &lt;i&gt;share&lt;/i&gt; в DOS-подобной ОС или &lt;tt&gt;mount -t vboxsf share mount_point&lt;/tt&gt; для доступа из Линукс-подобной ОС. Требует установки Дополнений гостевой ОС.</translation>
    </message>
    <message>
        <source>Name</source>
        <translation>Имя</translation>
    </message>
    <message>
        <source>Path</source>
        <translation>Путь</translation>
    </message>
    <message>
        <source>Access</source>
        <translation>Доступ</translation>
    </message>
    <message>
        <source> Global Folders</source>
        <translation type="obsolete">Глобальные папки</translation>
    </message>
    <message>
        <source>&amp;Add Shared Folder</source>
        <translation>&amp;Добавить общую папку</translation>
    </message>
    <message>
        <source>&amp;Edit Shared Folder</source>
        <translation>&amp;Изменить общую папку</translation>
    </message>
    <message>
        <source>&amp;Remove Shared Folder</source>
        <translation>&amp;Удалить общую папку</translation>
    </message>
    <message>
        <source>&amp;Folders List</source>
        <translation>&amp;Список общих папок</translation>
    </message>
    <message>
        <source>Auto-Mount</source>
        <translation>Авто-подключение</translation>
    </message>
    <message>
        <source>Yes</source>
        <translation>Да</translation>
    </message>
</context>
<context>
    <name>UIMachineSettingsSFDetails</name>
    <message>
        <source>Add Share</source>
        <translation>Добавить общую папку</translation>
    </message>
    <message>
        <source>Edit Share</source>
        <translation>Изменить общую папку</translation>
    </message>
    <message>
        <source>Dialog</source>
        <translation></translation>
    </message>
    <message>
        <source>Folder Path:</source>
        <translation>Путь к папке:</translation>
    </message>
    <message>
        <source>Folder Name:</source>
        <translation>Имя папки:</translation>
    </message>
    <message>
        <source>Displays the name of the shared folder (as it will be seen by the guest OS).</source>
        <translation>Показывает имя общей папки (под этим именем папка будет видна в гостевой ОС).</translation>
    </message>
    <message>
        <source>When checked, the guest OS will not be able to write to the specified shared folder.</source>
        <translation>Когда стоит галочка, гостевая ОС будет лишена права записи в указанную общую папку.</translation>
    </message>
    <message>
        <source>&amp;Read-only</source>
        <translation>&amp;Только для чтения</translation>
    </message>
    <message>
        <source>&amp;Make Permanent</source>
        <translation>&amp;Создать постоянную папку</translation>
    </message>
    <message>
        <source>When checked, the guest OS will try to automatically mount the shared folder on startup.</source>
        <translation>Если стоит галочка, гостевая ОС будет пытаться автоматически подключать указанную общую папку в процессе загрузки.</translation>
    </message>
    <message>
        <source>&amp;Auto-mount</source>
        <translation>&amp;Авто-подключение</translation>
    </message>
    <message>
        <source>If checked, this shared folder will be permanent.</source>
        <translation>Если стоит галочка, выбранная общая папка будет постоянной для данной машины.</translation>
    </message>
</context>
<context>
    <name>UIMachineSettingsSerial</name>
    <message>
        <source>Port %1</source>
        <comment>serial ports</comment>
        <translation>Порт %1</translation>
    </message>
    <message>
        <source>When checked, enables the given serial port of the virtual machine.</source>
        <translation>Когда стоит галочка, активизируется указанный последовательный порт виртуальной машины.</translation>
    </message>
    <message>
        <source>&amp;Enable Serial Port</source>
        <translation>&amp;Включить последовательный порт</translation>
    </message>
    <message>
        <source>Port &amp;Number:</source>
        <translation>&amp;Номер порта:</translation>
    </message>
    <message>
        <source>Displays the serial port number. You can choose one of the standard serial ports or select &lt;b&gt;User-defined&lt;/b&gt; and specify port parameters manually.</source>
        <translation>Задает номер последовательного порта. Вы можете выбрать один из стандартных номеров портов или выбрать &lt;b&gt;Пользовательский&lt;/b&gt; и указать параметры порта вручную.</translation>
    </message>
    <message>
        <source>&amp;IRQ:</source>
        <translation>&amp;Прерывание:</translation>
    </message>
    <message>
        <source>Displays the IRQ number of this serial port. Valid values are integer numbers in range from &lt;tt&gt;0&lt;/tt&gt; to &lt;tt&gt;255&lt;/tt&gt;. Values greater than &lt;tt&gt;15&lt;/tt&gt; may only be used if the &lt;b&gt;IO APIC&lt;/b&gt; is enabled for this virtual machine.</source>
        <translation type="obsolete">Показывает номер прерывания (IRQ) для этого последовательного порта. Допустимые значения -- целые числа в диапазоне от &lt;tt&gt;0&lt;/tt&gt; до &lt;tt&gt;255&lt;/tt&gt;. Значения больше &lt;tt&gt;15&lt;/tt&gt; могут использоваться только в том случае, если для этой машины включен &lt;b&gt;IO APIC&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>I/O Po&amp;rt:</source>
        <translation>Пор&amp;т В/В:</translation>
    </message>
    <message>
        <source>Displays the base I/O port address of this serial port. This should be a whole number between &lt;tt&gt;0&lt;/tt&gt; and &lt;tt&gt;0xFFFF&lt;/tt&gt;.</source>
        <translation type="obsolete">Показывает базовый адрес ввода-вывода для этого последовательного порта. Допустимые значения -- целые числа в диапазоне от &lt;tt&gt;0&lt;/tt&gt; до &lt;tt&gt;0xFFFF&lt;/tt&gt;.</translation>
    </message>
    <message>
        <source>Port &amp;Mode:</source>
        <translation>&amp;Режим порта:</translation>
    </message>
    <message>
        <source>Controls the working mode of this serial port. If you select &lt;b&gt;Disconnected&lt;/b&gt;, the guest OS will detect the serial port but will not be able to operate it.</source>
        <translation>Управляет режимом работы последовательного порта. Если выбрать &lt;b&gt;Отключен&lt;/b&gt;, то гостевая ОС обнаружит последовательный порт, но не сможет с ним работать.</translation>
    </message>
    <message>
        <source>If checked, the pipe specified in the &lt;b&gt;Port Path&lt;/b&gt; field will be created by the virtual machine when it starts. Otherwise, the virtual machine will assume that the pipe exists and try to use it.</source>
        <translation>Если стоит галочка, то канал, указанный в поле &lt;b&gt;Путь к порту&lt;/b&gt;, будет создан при старте виртуальой машины. В противном случае, виртуальная машина попытается использовать существующий канал.</translation>
    </message>
    <message>
        <source>&amp;Create Pipe</source>
        <translation>&amp;Создать канал</translation>
    </message>
    <message>
        <source>Port &amp;Path:</source>
        <translation type="obsolete">П&amp;уть к порту:</translation>
    </message>
    <message>
        <source>Displays the path to the serial port&apos;s pipe on the host when the port is working in &lt;b&gt;Host Pipe&lt;/b&gt; mode, or the host serial device name when the port is working in &lt;b&gt;Host Device&lt;/b&gt; mode.</source>
        <translation>Показывает путь к каналу последовательного порта на основном ПК, когда порт работает в режиме &lt;b&gt;Хост-канал&lt;/b&gt;, либо имя устройства последовательного порта основного ПК, когда порт работает в режиме &lt;b&gt;Хост-устройство&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Port/File &amp;Path:</source>
        <translation>П&amp;уть к порту/файлу:</translation>
    </message>
    <message>
        <source>Displays the IRQ number of this serial port. This should be a whole number between &lt;tt&gt;0&lt;/tt&gt; and &lt;tt&gt;255&lt;/tt&gt;. Values greater than &lt;tt&gt;15&lt;/tt&gt; may only be used if the &lt;b&gt;IO APIC&lt;/b&gt; setting is enabled for this virtual machine.</source>
        <translation>Показывает номер прерывания (IRQ) для этого последовательного порта. Допустимые значения -- целые числа в диапазоне от &lt;tt&gt;0&lt;/tt&gt; до &lt;tt&gt;255&lt;/tt&gt;. Значения больше &lt;tt&gt;15&lt;/tt&gt; могут использоваться только в том случае, если для этой машины включен &lt;b&gt;IO APIC&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Displays the base I/O port address of this serial port. Valid values are integer numbers in range from &lt;tt&gt;0&lt;/tt&gt; to &lt;tt&gt;0xFFFF&lt;/tt&gt;.</source>
        <translation>Показывает базовый адрес ввода-вывода для этого последовательного порта. Допустимые значения -- целые числа в диапазоне от &lt;tt&gt;0&lt;/tt&gt; до &lt;tt&gt;0xFFFF&lt;/tt&gt;.</translation>
    </message>
</context>
<context>
    <name>UIMachineSettingsSerialPage</name>
    <message>
        <source>Duplicate port number selected </source>
        <translation>Выбран повторяющийся номер порта</translation>
    </message>
    <message>
        <source>Port path not specified </source>
        <translation>Не задан путь к порту</translation>
    </message>
    <message>
        <source>Duplicate port path entered </source>
        <translation>Введен повторяющийся путь к порту</translation>
    </message>
</context>
<context>
    <name>UIMachineSettingsStorage</name>
    <message>
        <source>No hard disk is selected for &lt;i&gt;%1&lt;/i&gt;</source>
        <translation type="obsolete">Не выбран жесткий диск для подключения к &lt;i&gt;%1&lt;/i&gt;</translation>
    </message>
    <message>
        <source>&lt;i&gt;%1&lt;/i&gt; uses the hard disk that is already attached to &lt;i&gt;%2&lt;/i&gt;</source>
        <translation type="obsolete">&lt;i&gt;%1&lt;/i&gt; использует жесткий диск, который уже подключен к &lt;i&gt;%2&lt;/i&gt;</translation>
    </message>
    <message>
        <source>&amp;Add Attachment</source>
        <translation type="obsolete">&amp;Добавить подключение</translation>
    </message>
    <message>
        <source>&amp;Remove Attachment</source>
        <translation type="obsolete">&amp;Удалить подключение</translation>
    </message>
    <message>
        <source>&amp;Select Hard Disk</source>
        <translation type="obsolete">&amp;Выбрать жесткий диск</translation>
    </message>
    <message>
        <source>Adds a new hard disk attachment.</source>
        <translation type="obsolete">Создает новое подключение жесткого диска.</translation>
    </message>
    <message>
        <source>Removes the highlighted hard disk attachment.</source>
        <translation type="obsolete">Отсоединяет выбранный жесткий диск.</translation>
    </message>
    <message>
        <source>When checked, enables the virtual SATA controller of this machine. Note that you cannot attach hard disks to SATA ports when the virtual SATA controller is disabled.</source>
        <translation type="obsolete">Когда стоит галочка, включается виртуальный SATA-контроллер этой машины. Имейте в виду, что вы не сможете подсоединить жесткие диски к портам SATA, если виртуальный SATA-контроллер выключен.</translation>
    </message>
    <message>
        <source>&amp;Enable SATA Controller</source>
        <translation type="obsolete">&amp;Включить контроллер SATA</translation>
    </message>
    <message>
        <source>&amp;Attachments</source>
        <translation type="obsolete">&amp;Подключения</translation>
    </message>
    <message>
        <source>Lists all hard disks attached to this machine. Use a mouse click or the &lt;tt&gt;Space&lt;/tt&gt; key on the highlighted item to activate the drop-down list and choose the desired value. Use the context menu or buttons to the right to add or remove hard disk attachments.</source>
        <translation type="obsolete">Перечисляет все жесткие диски, подсоединенные к этой машине. Дважды щелкние мышью или нажмите клавишу &lt;tt&gt;Space&lt;/tt&gt; на выбранном элементе для выбора нужного значения из выпадающего списка. Используйте контекстное меню или кнопки справа для добавления или удаления подключений.</translation>
    </message>
    <message>
        <source>Invokes the Virtual Media Manager to select a hard disk to attach to the currently highlighted slot.</source>
        <translation type="obsolete">Вызывает Менеджер виртуальных носителей для выбора жесткого диска, подключаемого к указанному разъему.</translation>
    </message>
    <message>
        <source>If checked, shows the differencing hard disks that are attached to slots rather than their base hard disks (shown for indirect attachments) and allows explicit attaching of differencing hard disks. Check this only if you need a complex hard disk setup.</source>
        <translation type="obsolete">Если стоит галочка, Вы увидите разностные диски, которые на самом деле подсоединены к слотам (вместо базовых дисков, показанных в случае косвенных подсоединений), а также сможете подсоединять другие разностные диски напрямую. Это нужно только для сложных конфигураций.</translation>
    </message>
    <message>
        <source>&amp;Show Differencing Hard Disks</source>
        <translation type="obsolete">&amp;Показывать разностные жесткие диски</translation>
    </message>
    <message>
        <source>When checked, enables an additional virtual controller (either SATA or SCSI) of this machine.</source>
        <translation type="obsolete">Если стоит галочка, для данной машины будет включен дополнительный виртуальный контроллер (SATA либо SCSI).</translation>
    </message>
    <message>
        <source>&amp;Enable Additional Controller</source>
        <translation type="obsolete">&amp;Включить дополнительный контроллер</translation>
    </message>
    <message>
        <source>IDE &amp;Controller Type</source>
        <translation type="obsolete">&amp;Тип контроллера IDE</translation>
    </message>
    <message>
        <source>Defines the type of the virtual IDE controller. Depending on this value, VirtualBox will provide different virtual IDE hardware devices to the guest OS.</source>
        <translation type="obsolete">Задает тип виртуального контроллера IDE. В зависимости от выбранного значения, VirtualBox обеспечит виртуальную машину соответствующим IDE-устройством.</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;Bus:&amp;nbsp;&amp;nbsp;%2&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;Type:&amp;nbsp;&amp;nbsp;%3&lt;/nobr&gt;</source>
        <translation>&lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;Шина:&amp;nbsp;&amp;nbsp;%2&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;Тип:&amp;nbsp;&amp;nbsp;%3&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;Expand/Collapse&amp;nbsp;Item&lt;/nobr&gt;</source>
        <translation>&lt;nobr&gt;Раскрыть/Скрыть&amp;nbsp;элемент&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;Add&amp;nbsp;Hard&amp;nbsp;Disk&lt;/nobr&gt;</source>
        <translation>&lt;nobr&gt;Добавить&amp;nbsp;жёсткий&amp;nbsp;диск&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;Add&amp;nbsp;CD/DVD&amp;nbsp;Device&lt;/nobr&gt;</source>
        <translation>&lt;nobr&gt;Добавить&amp;nbsp;привод&amp;nbsp;оптических&amp;nbsp;дисков&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;Add&amp;nbsp;Floppy&amp;nbsp;Device&lt;/nobr&gt;</source>
        <translation>&lt;nobr&gt;Добавить&amp;nbsp;привод&amp;nbsp;гибких&amp;nbsp;дисков&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>No hard disk is selected for &lt;i&gt;%1&lt;/i&gt;.</source>
        <translation type="obsolete">Не выбран жёсткий диск для &lt;i&gt;%1&lt;/i&gt;.</translation>
    </message>
    <message>
        <source>&lt;i&gt;%1&lt;/i&gt; uses a medium that is already attached to &lt;i&gt;%2&lt;/i&gt;.</source>
        <translation>&lt;i&gt;%1&lt;/i&gt; использует устройство, которое уже подключено к &lt;i&gt;%2&lt;/i&gt;.</translation>
    </message>
    <message>
        <source>Add Controller</source>
        <translation>Добавить контроллер</translation>
    </message>
    <message>
        <source>Add IDE Controller</source>
        <translation>Добавить IDE контроллер</translation>
    </message>
    <message>
        <source>Add SATA Controller</source>
        <translation>Добавить SATA контроллер</translation>
    </message>
    <message>
        <source>Add SCSI Controller</source>
        <translation>Добавить SCSI контроллер</translation>
    </message>
    <message>
        <source>Add Floppy Controller</source>
        <translation>Добавить Floppy контроллер</translation>
    </message>
    <message>
        <source>Remove Controller</source>
        <translation>Удалить контроллер</translation>
    </message>
    <message>
        <source>Add Attachment</source>
        <translation>Добавить устройство</translation>
    </message>
    <message>
        <source>Add Hard Disk</source>
        <translation>Добавить жёсткий диск</translation>
    </message>
    <message>
        <source>Add CD/DVD Device</source>
        <translation>Добавить привод оптических дисков</translation>
    </message>
    <message>
        <source>Add Floppy Device</source>
        <translation>Добавить привод гибких дисков</translation>
    </message>
    <message>
        <source>Remove Attachment</source>
        <translation>Удалить устройство</translation>
    </message>
    <message>
        <source>Adds a new controller to the end of the Storage Tree.</source>
        <translation>Добавить новый контроллер к дереву носителей информации.</translation>
    </message>
    <message>
        <source>Removes the controller highlighted in the Storage Tree.</source>
        <translation>Удалить контроллер, выбранный в дереве носителей информации.</translation>
    </message>
    <message>
        <source>Adds a new attachment to the Storage Tree using currently selected controller as parent.</source>
        <translation>Добавить новое устройство к выбранному контроллеру.</translation>
    </message>
    <message>
        <source>Removes the attachment highlighted in the Storage Tree.</source>
        <translation>Удалить выбранное устройство.</translation>
    </message>
    <message>
        <source>IDE Controller</source>
        <translation>IDE контроллер</translation>
    </message>
    <message>
        <source>SATA Controller</source>
        <translation>SATA контроллер</translation>
    </message>
    <message>
        <source>SCSI Controller</source>
        <translation>SCSI контроллер</translation>
    </message>
    <message>
        <source>Floppy Controller</source>
        <translation>Floppy контроллер</translation>
    </message>
    <message>
        <source>Hard &amp;Disk:</source>
        <translation>&amp;Жёсткий диск:</translation>
    </message>
    <message>
        <source>&amp;CD/DVD Device:</source>
        <translation type="obsolete">&amp;Привод:</translation>
    </message>
    <message>
        <source>&amp;Floppy Device:</source>
        <translation type="obsolete">&amp;Привод:</translation>
    </message>
    <message>
        <source>&amp;Storage Tree</source>
        <translation>&amp;Носители информации</translation>
    </message>
    <message>
        <source>Contains all storage controllers for this machine and the virtual images and host drives attached to them.</source>
        <translation>Данное дерево содержит все контроллеры носителей информации, подключенные к данной виртуальной машине, а так же образы виртуальных дисков и приводы хоста, подключенные к этим контроллерам.</translation>
    </message>
    <message>
        <source>Information</source>
        <translation>Информация</translation>
    </message>
    <message>
        <source>The Storage Tree can contain several controllers of different types. This machine currently has no controllers.</source>
        <translation>Дерево носителей информации может содержать несколько контроллеров различных типов. Данная машина не имеет подключенных контроллеров.</translation>
    </message>
    <message>
        <source>Attributes</source>
        <translation>Атрибуты</translation>
    </message>
    <message>
        <source>&amp;Name:</source>
        <translation>&amp;Имя:</translation>
    </message>
    <message>
        <source>Changes the name of the storage controller currently selected in the Storage Tree.</source>
        <translation>Задаёт имя контроллера носителей информации, выбранного в данный момент.</translation>
    </message>
    <message>
        <source>&amp;Type:</source>
        <translation>&amp;Тип:</translation>
    </message>
    <message>
        <source>Selects the sub-type of the storage controller currently selected in the Storage Tree.</source>
        <translation>Задаёт тип контроллера носителей информации, выбранного в данный момент.</translation>
    </message>
    <message>
        <source>S&amp;lot:</source>
        <translation type="obsolete">&amp;Слот:</translation>
    </message>
    <message>
        <source>Selects the slot on the storage controller used by this attachment. The available slots depend on the type of the controller and other attachments on it.</source>
        <translation>Задаёт слот контроллера носителей информации, используемый данным виртуальным устройством.</translation>
    </message>
    <message>
        <source>Selects the virtual disk image or the host drive used by this attachment.</source>
        <translation type="obsolete">Задаёт виртуальный образ либо привод хоста, используемый данным виртуальным устройством.</translation>
    </message>
    <message>
        <source>Opens the Virtual Media Manager to select a virtual image for this attachment.</source>
        <translation type="obsolete">Открывает менеджер виртуальных носителей для выбора образа диска, используемого данным виртуальным устройством.</translation>
    </message>
    <message>
        <source>Open Virtual Media Manager</source>
        <translation type="obsolete">Открыть менеджер виртуальных носителей</translation>
    </message>
    <message>
        <source>D&amp;ifferencing Disks</source>
        <translation type="obsolete">Показывать &amp;разностные диски</translation>
    </message>
    <message>
        <source>When checked, allows the guest to send ATAPI commands directly to the host-drive which makes it possible to use CD/DVD writers connected to the host inside the VM. Note that writing audio CD inside the VM is not yet supported.</source>
        <translation>Когда стоит галочка, гостевой ОС разрешается посылать ATAPI-команды напрямую в физический привод, что делает возможным использовать подключенные к основному ПК устройства для записи CD/DVD внутри ВМ. Имейте в виду, что запись аудио-CD внутри ВМ пока еще не поддерживается.</translation>
    </message>
    <message>
        <source>&amp;Passthrough</source>
        <translation>&amp;Разрешить прямой доступ</translation>
    </message>
    <message>
        <source>Virtual Size:</source>
        <translation>Виртуальный размер:</translation>
    </message>
    <message>
        <source>Actual Size:</source>
        <translation>Реальный размер:</translation>
    </message>
    <message>
        <source>Size:</source>
        <translation>Размер:</translation>
    </message>
    <message>
        <source>Location:</source>
        <translation>Расположение:</translation>
    </message>
    <message>
        <source>Type (Format):</source>
        <translation>Тип (Формат):</translation>
    </message>
    <message>
        <source>Attached To:</source>
        <translation>Подсоединён к:</translation>
    </message>
    <message>
        <source>Allows to use host I/O caching capabilities.</source>
        <translation>Позволяет использовать функции кеширования операций ввода/вывода данного хоста.</translation>
    </message>
    <message>
        <source>Use host I/O cache</source>
        <translation>Кеширование операций ввода/вывода</translation>
    </message>
    <message>
        <source>Add SAS Controller</source>
        <translation>Добавить SAS контроллер</translation>
    </message>
    <message>
        <source>SAS Controller</source>
        <translation>SAS контроллер</translation>
    </message>
    <message>
        <source>Storage Controller</source>
        <translation type="obsolete">Контроллер</translation>
    </message>
    <message>
        <source>Storage Controller 1</source>
        <translation type="obsolete">Контроллер 1</translation>
    </message>
    <message>
        <source>Type:</source>
        <translation>Тип:</translation>
    </message>
    <message>
        <source>Host Drive</source>
        <translation>Физический привод</translation>
    </message>
    <message>
        <source>Image</source>
        <translation>Образ</translation>
    </message>
    <message>
        <source>Choose or create a virtual hard disk file. The virtual machine will see the data in the file as the contents of the virtual hard disk.</source>
        <translation>Выбрать/создать файл виртуального образа жёсткого диска. Виртуальная машина получит доступ к информации, содержащейся в файле, как если бы эта информация находилась на жёстком диске виртуальной машины.</translation>
    </message>
    <message>
        <source>Set up the virtual hard disk</source>
        <translation>Настроить жёсткий диск</translation>
    </message>
    <message>
        <source>CD/DVD &amp;Drive:</source>
        <translation>&amp;Привод:</translation>
    </message>
    <message>
        <source>Choose a virtual CD/DVD disk or a physical drive to use with the virtual drive. The virtual machine will see a disk inserted into the drive with the data in the file or on the disk in the physical drive as its contents.</source>
        <translation>Выбрать файл образа оптического диска или привод хоста для использования в виртуальном приводе гостевой ВМ. Виртуальная машина получит доступ к информации, содержащейся в образе или на носителе привода хоста, как если бы эта информация находилась на оптическом диске внутри виртуальной машины.</translation>
    </message>
    <message>
        <source>Set up the virtual CD/DVD drive</source>
        <translation>Настроить привод оптических дисков</translation>
    </message>
    <message>
        <source>Floppy &amp;Drive:</source>
        <translation>&amp;Дисковод:</translation>
    </message>
    <message>
        <source>Choose a virtual floppy disk or a physical drive to use with the virtual drive. The virtual machine will see a disk inserted into the drive with the data in the file or on the disk in the physical drive as its contents.</source>
        <translation>Выбрать файл образа гибкого диска или привод хоста для использования в виртуальном приводе гостевой ВМ. Виртуальная машина получит доступ к информации, содержащейся в образе или на носителе привода хоста, как если бы эта информация находилась на гибком диске внутри виртуальной машины.</translation>
    </message>
    <message>
        <source>Set up the virtual floppy drive</source>
        <translation>Настроить привод гибких дисков</translation>
    </message>
    <message>
        <source>Create a new hard disk...</source>
        <translation>Создать новый жёсткий диск...</translation>
    </message>
    <message>
        <source>Choose a virtual hard disk file...</source>
        <translation>Выбрать образ жёсткого диска...</translation>
    </message>
    <message>
        <source>Choose a virtual CD/DVD disk file...</source>
        <translation>Выбрать образ оптического диска...</translation>
    </message>
    <message>
        <source>Remove disk from virtual drive</source>
        <translation>Изъять диск из привода</translation>
    </message>
    <message>
        <source>Choose a virtual floppy disk file...</source>
        <translation>Выбрать образ гибкого диска...</translation>
    </message>
    <message>
        <source>When checked the virtual disk will not be removed when the guest system ejects it.</source>
        <translation>Если стоит галочка, VirtualBox будет подавлять демонтирование образа в случаях извлечения его со стороны гостевой системы.</translation>
    </message>
    <message>
        <source>&amp;Live CD/DVD</source>
        <translation>&amp;Живой CD/DVD</translation>
    </message>
    <message>
        <source>When checked the guest system will see the virtual disk as a solid state device.</source>
        <translation>Если стоит галочка, VirtualBox будет считать данное устройство твердотельным накопителем (SSD).</translation>
    </message>
    <message>
        <source>&amp;Solid-state drive</source>
        <translation>&amp;Твердотельный накопитель</translation>
    </message>
    <message>
        <source>Details:</source>
        <translation>Детали:</translation>
    </message>
    <message>
        <source>no name specified for controller at position &lt;b&gt;%1&lt;/b&gt;.</source>
        <translation>не указано имя контроллера на позиции &lt;b&gt;%1&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>controller at position &lt;b&gt;%1&lt;/b&gt; uses the name that is already used by controller at position &lt;b&gt;%2&lt;/b&gt;.</source>
        <translation>контроллер на позиции &lt;b&gt;%1&lt;/b&gt; использует имя, используемое контроллером на позиции &lt;b&gt;%2&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>no hard disk is selected for &lt;i&gt;%1&lt;/i&gt;.</source>
        <translation>не выбран жёсткий диск для &lt;i&gt;%1&lt;/i&gt;.</translation>
    </message>
    <message>
        <source>at most one supported</source>
        <comment>controller</comment>
        <translation>поддерживается максимум один</translation>
    </message>
    <message>
        <source>up to %1 supported</source>
        <comment>controllers</comment>
        <translation>поддерживается вплоть до %1</translation>
    </message>
    <message>
        <source>you are currently using more storage controllers than a %1 chipset supports. Please change the chipset type on the System settings page or reduce the number of the following storage controllers on the Storage settings page: %2.</source>
        <translation>В данный момент больше контроллеров носителей информации, чем поддерживается чипсетом %1. Пожалуйста, измените тип чипсета на странице &apos;Система&apos; или уменьшите количество следующих контроллеров на странице &apos;Носители&apos;: %2.</translation>
    </message>
    <message>
        <source>&amp;Port Count:</source>
        <translation>&amp;Порты:</translation>
    </message>
    <message>
        <source>Selects the port count of the SATA storage controller currently selected in the Storage Tree. This must be at least one more than the highest port number you need to use.</source>
        <translation>Задаёт количество портов контроллера носителей информации типа SATA, выбранного в данный момент в дереве носителей информации. Это значение не может быть меньше, чем максимальный номер использованного порта + 1.</translation>
    </message>
</context>
<context>
    <name>UIMachineSettingsSystem</name>
    <message>
        <source>you have assigned more than &lt;b&gt;%1%&lt;/b&gt; of your computer&apos;s memory (&lt;b&gt;%2&lt;/b&gt;) to the virtual machine. Not enough memory is left for your host operating system. Please select a smaller amount.</source>
        <translation>виртуальной машине назначено более &lt;b&gt;%1%&lt;/b&gt; памяти компьютера (&lt;b&gt;%2&lt;/b&gt;). Недостаточно памяти для операционной системы хоста. Задайте меньшее значение.</translation>
    </message>
    <message>
        <source>you have assigned more than &lt;b&gt;%1%&lt;/b&gt; of your computer&apos;s memory (&lt;b&gt;%2&lt;/b&gt;) to the virtual machine. There might not be enough memory left for your host operating system. Continue at your own risk.</source>
        <translation>виртуальной машине назначено более &lt;b&gt;%1%&lt;/b&gt; памяти компьютера (&lt;b&gt;%2&lt;/b&gt;). Для операционной системы хоста может оказаться недостаточно памяти. Продолжайте на свой страх и риск.</translation>
    </message>
    <message>
        <source>for performance reasons, the number of virtual CPUs attached to the virtual machine may not be more than twice the number of physical CPUs on the host (&lt;b&gt;%1&lt;/b&gt;). Please reduce the number of virtual CPUs.</source>
        <translation>в целях производительности, число виртуальных процессоров, подсоединённых к виртуальной машине, не может превышать число реальных процессоров хоста (&lt;b&gt;%1&lt;/b&gt;) более чем в два раза. Пожалуйста, задайте меньшее число виртуальных процессоров.</translation>
    </message>
    <message>
        <source>you have assigned more virtual CPUs to the virtual machine than the number of physical CPUs on your host system (&lt;b&gt;%1&lt;/b&gt;). This is likely to degrade the performance of your virtual machine. Please consider reducing the number of virtual CPUs.</source>
        <translation>для данной машины заданно число виртуальных процессоров, превышающее число реальных процессоров хоста (&lt;b&gt;%1&lt;/b&gt;). Это может отрицательно отразиться на производительности виртуальной машины. Имеет смысл уменьшить число виртуальных процессоров данной машины.</translation>
    </message>
    <message>
        <source>you have assigned more than one virtual CPU to this VM. This will not work unless the IO-APIC feature is also enabled. This will be done automatically when you accept the VM Settings by pressing the OK button.</source>
        <translation>для данной машины выбрано более одного виртуального процессора, что в свою очередь требует активации функции IO-APIC, иначе виртуальные процессоры не будут активны. Таким образом, эта функция будет включена автоматически в момент сохранения настроек ВМ.</translation>
    </message>
    <message>
        <source>you have assigned more than one virtual CPU to this VM. This will not work unless hardware virtualization (VT-x/AMD-V) is also enabled. This will be done automatically when you accept the VM Settings by pressing the OK button.</source>
        <translation>для данной машины выбрано более одного виртуального процессора, что в свою очередь требует активации функций аппаратной виртуализации (VT-x/AMD-V), иначе виртуальные процессоры не будут активны. Таким образом, эта функция будет включена автоматически в момент сохранения настроек ВМ.</translation>
    </message>
    <message>
        <source>&lt;qt&gt;%1&amp;nbsp;MB&lt;/qt&gt;</source>
        <translation>&lt;qt&gt;%1&amp;nbsp;МБ&lt;/qt&gt;</translation>
    </message>
    <message>
        <source>&lt;qt&gt;%1&amp;nbsp;CPU&lt;/qt&gt;</source>
        <comment>%1 is 1 for now</comment>
        <translation>&lt;qt&gt;%1&amp;nbsp;ЦПУ&lt;/qt&gt;</translation>
    </message>
    <message>
        <source>&lt;qt&gt;%1&amp;nbsp;CPUs&lt;/qt&gt;</source>
        <comment>%1 is 32 for now</comment>
        <translation type="obsolete">&lt;qt&gt;%1&amp;nbsp;ЦПУ&lt;/qt&gt;</translation>
    </message>
    <message>
        <source>&amp;Motherboard</source>
        <translation>&amp;Материнская плата</translation>
    </message>
    <message>
        <source>Base &amp;Memory:</source>
        <translation>&amp;Основная память:</translation>
    </message>
    <message>
        <source>Controls the amount of memory provided to the virtual machine. If you assign too much, the machine might not start.</source>
        <translation>Регулирует количество памяти, доступной для виртуальной машины. Если установить слишком большое значение, то машина может не запуститься.</translation>
    </message>
    <message>
        <source>MB</source>
        <translation>МБ</translation>
    </message>
    <message>
        <source>&amp;Boot Order:</source>
        <translation>По&amp;рядок загрузки:</translation>
    </message>
    <message>
        <source>Defines the boot device order. Use the checkboxes on the left to enable or disable individual boot devices. Move items up and down to change the device order.</source>
        <translation>Определяет порядок загрузочных устройств. Используйте галочки слева, чтобы разрешить или запретить загрузку с отдельных устройств. Порядок устройств изменяется перемещением их вверх и вниз.</translation>
    </message>
    <message>
        <source>Move Down (Ctrl-Down)</source>
        <translation>Вниз (Ctrl-Down)</translation>
    </message>
    <message>
        <source>Moves the selected boot device down.</source>
        <translation>Перемещает выбранное загрузочное устройство ниже по списку.</translation>
    </message>
    <message>
        <source>Move Up (Ctrl-Up)</source>
        <translation>Вверх (Ctrl-Up)</translation>
    </message>
    <message>
        <source>Moves the selected boot device up.</source>
        <translation>Перемещает выбранное загрузочное устройство выше по списку.</translation>
    </message>
    <message>
        <source>Extended Features:</source>
        <translation>Дополнительные возможности:</translation>
    </message>
    <message>
        <source>When checked, the virtual machine will support the Advanced Configuration and Power Management Interface (ACPI). &lt;b&gt;Note:&lt;/b&gt; don&apos;t disable this feature after having installed a Windows guest operating system!</source>
        <translation type="obsolete">Если стоит галочка, то виртуальная машина будет поддерживать улучшенный интерфейс для конфигурации и управления электропитанием (ACPI). &lt;b&gt;Примечание:&lt;/b&gt; невыключайте это свойство после установки Windows в качестве гостевой ОС!</translation>
    </message>
    <message>
        <source>Enable &amp;ACPI</source>
        <translation type="obsolete">&amp;Включить ACPI</translation>
    </message>
    <message>
        <source>When checked, the virtual machine will support the Input Output APIC (IO APIC), which may slightly decrease performance. &lt;b&gt;Note:&lt;/b&gt; don&apos;t disable this feature after having installed a Windows guest operating system!</source>
        <translation>Если стоит галочка, то виртуальная машина будет поддерживать операции ввода/вывода контроллера прерываний (IO APIC), что может слегка снизить производительность ВМ. &lt;b&gt;Примечание:&lt;/b&gt; не выключайте это свойство после установки Windows в качестве гостевой ОС!</translation>
    </message>
    <message>
        <source>Enable &amp;IO APIC</source>
        <translation>В&amp;ключить IO APIC</translation>
    </message>
    <message>
        <source>&amp;Processor</source>
        <translation>&amp;Процессор</translation>
    </message>
    <message>
        <source>&amp;Processor(s):</source>
        <translation>&amp;Процессор(ы):</translation>
    </message>
    <message>
        <source>Controls the number of virtual CPUs in the virtual machine.</source>
        <translation type="obsolete">Определяет число виртуальных процессоров виртуальной машины.</translation>
    </message>
    <message>
        <source>When checked, the Physical Address Extension (PAE) feature of the host CPU will be exposed to the virtual machine.</source>
        <translation>Если стоит галочка, виртуальной машине будет предоставлен доступ к функции Physical Address Extension (PAE, расширение физического адреса) центрального процессора основного ПК.</translation>
    </message>
    <message>
        <source>Enable PA&amp;E/NX</source>
        <translation>&amp;Включить PAE/NX</translation>
    </message>
    <message>
        <source>Acce&amp;leration</source>
        <translation>&amp;Ускорение</translation>
    </message>
    <message>
        <source>Hardware Virtualization:</source>
        <translation>Аппаратная виртуализация:</translation>
    </message>
    <message>
        <source>When checked, the virtual machine will try to make use of the host CPU&apos;s hardware virtualization extensions such as Intel VT-x and AMD-V.</source>
        <translation>Если стоит галочка, виртуальная машина будет пытаться задействовать расширенные функции аппаратной виртуализации процессора основного ПК, такие как Intel VT-x или AMD-V.</translation>
    </message>
    <message>
        <source>Enable &amp;VT-x/AMD-V</source>
        <translation>&amp;Включить VT-x/AMD-V</translation>
    </message>
    <message>
        <source>When checked, the virtual machine will try to make use of the nested paging extension of Intel VT-x and AMD-V.</source>
        <translation>Если стоит галочка, виртуальная машина будет пытаться использовать расширение Nested Paging для функций аппаратной виртуализации Intel VT-x and AMD-V.</translation>
    </message>
    <message>
        <source>Enable Nested Pa&amp;ging</source>
        <translation>В&amp;ключить  Nested Paging</translation>
    </message>
    <message>
        <source>&lt;qt&gt;%1&amp;nbsp;CPUs&lt;/qt&gt;</source>
        <comment>%1 is host cpu count * 2 for now</comment>
        <translation>&lt;qt&gt;%1&amp;nbsp;ЦПУ&lt;/qt&gt;</translation>
    </message>
    <message>
        <source>When checked, the guest will support the Extended Firmware Interface (EFI), which is required to boot certain guest OSes. Non-EFI aware OSes will not be able to boot if this option is activated.</source>
        <translation>Если стоит галочка, гостевая ОС будет поддерживать Extended Firmware Interface (EFI), что необходимо для загрузки некоторых гостевых ОС. Гостевые системы, которые не поддерживают EFI, не будут иметь возможности загрузиться в случае выбора данной опции.</translation>
    </message>
    <message>
        <source>Enable &amp;EFI (special OSes only)</source>
        <translation>Включить &amp;EFI (только специальные ОС)</translation>
    </message>
    <message>
        <source>If checked, the RTC device will report the time in UTC, otherwise in local (host) time. Unix usually expects the hardware clock to be set to UTC.</source>
        <translation>Если стоит галочка, часы Вашего хоста отобразят время по шкале всемирного координированного времени (UTC), иначе будет отображено локальное время хоста. Unix-подобные системы обычно придерживаются системы UTC.</translation>
    </message>
    <message>
        <source>Hardware clock in &amp;UTC time</source>
        <translation>Часы в системе &amp;UTC</translation>
    </message>
    <message>
        <source>Controls the number of virtual CPUs in the virtual machine. You need hardware virtualization support on your host system to use more than one virtual CPU.</source>
        <translation>Контролирует количество процессоров виртуальной машины. Вам необходима поддержка аппаратной виртуализации для задействования более одного процессора в виртуальной машине.</translation>
    </message>
    <message>
        <source>If checked, an absolute pointing device (a USB tablet) will be supported. Otherwise, only a standard PS/2 mouse will be emulated.</source>
        <translation>Если стоит галочка, будут поддерживаться абсолютные устройства позиционирования (такие как USB планшет). В противном случае, эмулированы будут лишь стандартные PS/2 мыши.</translation>
    </message>
    <message>
        <source>Enable &amp;absolute pointing device</source>
        <translation>&amp;Абсолютные устройства позиционирования</translation>
    </message>
    <message>
        <source>&amp;Chipset:</source>
        <translation>&amp;Чипсет:</translation>
    </message>
    <message>
        <source>Defines chipset type used in this VM.</source>
        <translation>Определяет набор микросхем (чипсет), используемый материнской платой данной виртуальной машины.</translation>
    </message>
    <message>
        <source>you have assigned ICH9 chipset type to this VM. It will not work properly unless the IO-APIC feature is also enabled. This will be done automatically when you accept the VM Settings by pressing the OK button.</source>
        <translation>Вы задали тип чипсета ICH9 для данной машины. Машина не сможет функционировать если функция IO-APIC будет отключена, поэтому данная функция будет включена автоматически при закрытии диалога настроек.</translation>
    </message>
    <message>
        <source>&amp;Execution Cap:</source>
        <translation>Предел &amp;загрузки ЦПУ:</translation>
    </message>
    <message>
        <source>Limits the amount of time that each virtual CPU is allowed to run for. Each virtual CPU will be allowed to use up to this percentage of the processing time available on one physical CPU. The execution cap can be disabled by setting it to 100%. Setting the cap too low can make the machine feel slow to respond.</source>
        <translation>Ограничивает количество времени, отведённого каждому виртуальному процессору. Каждому виртуальному процессору будет отведено вплоть до указанного количества времени работы реального процессора (в процентах). Это ограничение снимается путём установки данного атрибута в значение, равное 100%. Установка данного атрибута в слишком малое значение может привести к очень медленной работе виртуальной машины.</translation>
    </message>
    <message>
        <source>you have set the processor execution cap to a low value. This can make the machine feel slow to respond.</source>
        <translation>Вы установили предел загрузки ЦПУ в слишком малое значение, что может привести к очень медленной работе виртуальной машины.</translation>
    </message>
    <message>
        <source>you have enabled a USB HID (Human Interface Device). This will not work unless USB emulation is also enabled. This will be done automatically when you accept the VM Settings by pressing the OK button.</source>
        <translation>Вы включили поддержку USB HID (устройства пользовательского интерфейса). Данная опция не работает без активированной USB эмуляции, поэтому USB эмуляция будет активирована в момент сохранения настроек виртуальной машины при закрытии данного диалога.</translation>
    </message>
    <message>
        <source>&lt;qt&gt;%1%&lt;/qt&gt;</source>
        <comment>Min CPU execution cap in %</comment>
        <translation>&lt;qt&gt;%1%&lt;/qt&gt;</translation>
    </message>
    <message>
        <source>&lt;qt&gt;%1%&lt;/qt&gt;</source>
        <comment>Max CPU execution cap in %</comment>
        <translation>&lt;qt&gt;%1%&lt;/qt&gt;</translation>
    </message>
</context>
<context>
    <name>UIMachineSettingsUSB</name>
    <message>
        <source>&amp;Add Empty Filter</source>
        <translation>&amp;Добавить пустой фильтр</translation>
    </message>
    <message>
        <source>A&amp;dd Filter From Device</source>
        <translation>Д&amp;обавить из устройства</translation>
    </message>
    <message>
        <source>&amp;Edit Filter</source>
        <translation>&amp;Изменить фильтр</translation>
    </message>
    <message>
        <source>&amp;Remove Filter</source>
        <translation>&amp;Удалить фильтр</translation>
    </message>
    <message>
        <source>&amp;Move Filter Up</source>
        <translation>&amp;Переместить вверх</translation>
    </message>
    <message>
        <source>M&amp;ove Filter Down</source>
        <translation>П&amp;ереместить вниз</translation>
    </message>
    <message>
        <source>Adds a new USB filter with all fields initially set to empty strings. Note that such a filter will match any attached USB device.</source>
        <translation>Добавляет новый USB-фильтр, в котором все поля первоначально пустые. Имейте ввиду, что пустой фильтр будет соответствовать любому подсоединенному USB-устройству.</translation>
    </message>
    <message>
        <source>Adds a new USB filter with all fields set to the values of the selected USB device attached to the host PC.</source>
        <translation>Добавляет новый USB-фильтр, в котором все поля заполнены значениями одного из USB-устройств, подключенных к основному ПК.</translation>
    </message>
    <message>
        <source>Edits the selected USB filter.</source>
        <translation>Изменяет свойства выбранного USB-фильтра.</translation>
    </message>
    <message>
        <source>Removes the selected USB filter.</source>
        <translation>Удаляет выбранный USB-фильтр.</translation>
    </message>
    <message>
        <source>Moves the selected USB filter up.</source>
        <translation>Перемещает выбранный USB-фильтр вверх.</translation>
    </message>
    <message>
        <source>Moves the selected USB filter down.</source>
        <translation>Перемещает выбранный USB-фильтр вниз.</translation>
    </message>
    <message>
        <source>New Filter %1</source>
        <comment>usb</comment>
        <translation>Новый фильтр %1</translation>
    </message>
    <message>
        <source>When checked, enables the virtual USB controller of this machine.</source>
        <translation>Когда стоит галочка, активизируется виртуальный USB-контроллер этой машины.</translation>
    </message>
    <message>
        <source>Enable &amp;USB Controller</source>
        <translation>&amp;Включить контроллер USB</translation>
    </message>
    <message>
        <source>When checked, enables the virtual USB EHCI controller of this machine. The USB EHCI controller provides USB 2.0 support.</source>
        <translation>Когда стоит галочка, активизируется контроллер USB EHCI для этой машины. Контроллер USB EHCI предоставляет поддержку USB 2.0.</translation>
    </message>
    <message>
        <source>Enable USB 2.0 (E&amp;HCI) Controller</source>
        <translation>Включить контроллер USB &amp;2.0 (EHCI)</translation>
    </message>
    <message>
        <source>USB Device &amp;Filters</source>
        <translation>Фильтры &amp;устройств USB</translation>
    </message>
    <message>
        <source>Lists all USB filters of this machine. The checkbox to the left defines whether the particular filter is enabled or not. Use the context menu or buttons to the right to add or remove USB filters.</source>
        <translation>Показывает список всех USB-фильтров этой машины. Галочка слева указывает, включен данный фильтр или нет. Используйте контекстное меню или кнопки справа для добавления или удаления фильтров.</translation>
    </message>
    <message>
        <source>[filter]</source>
        <translation></translation>
    </message>
    <message>
        <source>&lt;nobr&gt;Vendor ID: %1&lt;/nobr&gt;</source>
        <comment>USB filter tooltip</comment>
        <translation>&lt;nobr&gt;ID поставщика:  %1&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;Product ID: %2&lt;/nobr&gt;</source>
        <comment>USB filter tooltip</comment>
        <translation>&lt;nobr&gt;ID продукта: %2&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;Revision: %3&lt;/nobr&gt;</source>
        <comment>USB filter tooltip</comment>
        <translation>&lt;nobr&gt;Ревизия: %3&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;Product: %4&lt;/nobr&gt;</source>
        <comment>USB filter tooltip</comment>
        <translation>&lt;nobr&gt;Продукт: %4&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;Manufacturer: %5&lt;/nobr&gt;</source>
        <comment>USB filter tooltip</comment>
        <translation>&lt;nobr&gt;Производитель: %5&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;Serial No.: %1&lt;/nobr&gt;</source>
        <comment>USB filter tooltip</comment>
        <translation>&lt;nobr&gt;Серийный №: %1&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;Port: %1&lt;/nobr&gt;</source>
        <comment>USB filter tooltip</comment>
        <translation>&lt;nobr&gt;Порт: %1&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;State: %1&lt;/nobr&gt;</source>
        <comment>USB filter tooltip</comment>
        <translation>&lt;nobr&gt;Состояние: %1&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>USB 2.0 is currently enabled for this virtual machine. However this requires the &lt;b&gt;%1&lt;/b&gt; to be installed. Please install the Extension Pack from the VirtualBox download site. After this you will be able to re-enable USB 2.0. It will be disabled in the meantime unless you cancel the current settings changes.</source>
        <translation type="obsolete">в настоящий момент, поддержка USB 2.0 активна для данной виртуальной машины. Однако, для того, чтобы она работала верно, необходимо установить плагин &lt;b&gt;%1&lt;/b&gt;. Пожалуйста, установите плагин, предварительно загрузив его с сайта поддержки VirtualBox. После этого Вы сможете повторно активировать поддержку USB 2.0. До установки плагина поддержка USB 2.0 будет автоматически отключаться по принятии настроек данной машины.</translation>
    </message>
    <message>
        <source>USB 2.0 is currently enabled for this virtual machine. However, this requires the &lt;b&gt;%1&lt;/b&gt; to be installed. Please install the Extension Pack from the VirtualBox download site. After this you will be able to re-enable USB 2.0. It will be disabled in the meantime unless you cancel the current settings changes.</source>
        <translation>в настоящий момент, поддержка USB 2.0 активна для данной виртуальной машины. Однако, для того, чтобы она работала верно, необходимо установить плагин &lt;b&gt;%1&lt;/b&gt;. Пожалуйста, установите плагин, предварительно загрузив его с сайта поддержки VirtualBox. После этого Вы сможете повторно активировать поддержку USB 2.0. До установки плагина поддержка USB 2.0 будет автоматически отключаться по принятии настроек данной машины.</translation>
    </message>
</context>
<context>
    <name>UIMachineSettingsUSBFilterDetails</name>
    <message>
        <source>Any</source>
        <comment>remote</comment>
        <translation>Оба</translation>
    </message>
    <message>
        <source>Yes</source>
        <comment>remote</comment>
        <translation>Да</translation>
    </message>
    <message>
        <source>No</source>
        <comment>remote</comment>
        <translation>Нет</translation>
    </message>
    <message>
        <source>&amp;Name:</source>
        <translation>&amp;Имя:</translation>
    </message>
    <message>
        <source>Displays the filter name.</source>
        <translation>Показывает название фильтра.</translation>
    </message>
    <message>
        <source>&amp;Vendor ID:</source>
        <translation>ID &amp;поставщика:</translation>
    </message>
    <message>
        <source>Defines the vendor ID filter. The &lt;i&gt;exact match&lt;/i&gt; string format is &lt;tt&gt;XXXX&lt;/tt&gt; where &lt;tt&gt;X&lt;/tt&gt; is a hexadecimal digit. An empty string will match any value.</source>
        <translation>Задает фильтр по ID поставщика. Формат строки &lt;i&gt;точного соответствия&lt;/i&gt; - &lt;tt&gt;XXXX&lt;/tt&gt;, где &lt;tt&gt;X&lt;/tt&gt; - шестнадцатиричная цифра. Пустая строка соответствует любому значению.</translation>
    </message>
    <message>
        <source>&amp;Product ID:</source>
        <translation>ID про&amp;дукта:</translation>
    </message>
    <message>
        <source>Defines the product ID filter. The &lt;i&gt;exact match&lt;/i&gt; string format is &lt;tt&gt;XXXX&lt;/tt&gt; where &lt;tt&gt;X&lt;/tt&gt; is a hexadecimal digit. An empty string will match any value.</source>
        <translation>Задает фильтр по ID продукта. Формат строки &lt;i&gt;точного соответствия&lt;/i&gt; - &lt;tt&gt;XXXX&lt;/tt&gt;, где &lt;tt&gt;X&lt;/tt&gt; - шестнадцатиричная цифра. Пустая строка соответствует любомузначению.</translation>
    </message>
    <message>
        <source>&amp;Revision:</source>
        <translation>Р&amp;евизия:</translation>
    </message>
    <message>
        <source>Defines the revision number filter. The &lt;i&gt;exact match&lt;/i&gt; string format is &lt;tt&gt;IIFF&lt;/tt&gt; where &lt;tt&gt;I&lt;/tt&gt; is a decimal digit of the integer part and &lt;tt&gt;F&lt;/tt&gt; is a decimal digit of the fractional part. An empty string will match any value.</source>
        <translation>Задает фильтр по номеру ревизии. Формат строки &lt;i&gt;точного соответствия&lt;/i&gt; - &lt;tt&gt;IIFF&lt;/tt&gt;, где &lt;tt&gt;I&lt;/tt&gt; - десятичная цифра целой части, а &lt;tt&gt;F&lt;/tt&gt; - десятичная цифра дробной части. Пустая строка соответствует любому значению.</translation>
    </message>
    <message>
        <source>&amp;Manufacturer:</source>
        <translation>П&amp;роизводитель:</translation>
    </message>
    <message>
        <source>Defines the manufacturer filter as an &lt;i&gt;exact match&lt;/i&gt; string. An empty string will match any value.</source>
        <translation>Задает фильтр по производителю в виде строки с &lt;i&gt;точным соответствием&lt;/i&gt;. Пустая строка соответствует любому значению.</translation>
    </message>
    <message>
        <source>Pro&amp;duct:</source>
        <translation>Прод&amp;укт:</translation>
    </message>
    <message>
        <source>Defines the product name filter as an &lt;i&gt;exact match&lt;/i&gt; string. An empty string will match any value.</source>
        <translation>Задает фильтр по названию продукта в виде строки с &lt;i&gt;точным соответствием&lt;/i&gt;. Пустая строка соответствует любому значению.</translation>
    </message>
    <message>
        <source>&amp;Serial No.:</source>
        <translation>&amp;Серийный №:</translation>
    </message>
    <message>
        <source>Defines the serial number filter as an &lt;i&gt;exact match&lt;/i&gt; string. An empty string will match any value.</source>
        <translation>Задает фильтр по серийному номеру в виде строки с &lt;i&gt;точным соответствием&lt;/i&gt;. Пустая строка соответствует любому значению.</translation>
    </message>
    <message>
        <source>Por&amp;t:</source>
        <translation>Пор&amp;т:</translation>
    </message>
    <message>
        <source>Defines the host USB port filter as an &lt;i&gt;exact match&lt;/i&gt; string. An empty string will match any value.</source>
        <translation>Задает фильтр по физическому порту USB в виде строки &lt;i&gt;точного соответствия&lt;/i&gt;. Пустая строка соответствует любому значению.</translation>
    </message>
    <message>
        <source>R&amp;emote:</source>
        <translation>Уд&amp;аленное:</translation>
    </message>
    <message>
        <source>Defines whether this filter applies to USB devices attached locally to the host computer (&lt;i&gt;No&lt;/i&gt;), to a VRDP client&apos;s computer (&lt;i&gt;Yes&lt;/i&gt;), or both (&lt;i&gt;Any&lt;/i&gt;).</source>
        <translation>Определяет, применяется ли этот фильтр к USB-устройствам, подсоединенным локально к основному ПК (&lt;i&gt;Нет&lt;/i&gt;), к компьютеру VRDP-клиента (&lt;i&gt;Да&lt;/i&gt;), или к обоим (&lt;i&gt;Оба&lt;/i&gt;).</translation>
    </message>
    <message>
        <source>&amp;Action:</source>
        <translation>Де&amp;йствие:</translation>
    </message>
    <message>
        <source>Defines an action performed by the host computer when a matching device is attached: give it up to the host OS (&lt;i&gt;Ignore&lt;/i&gt;) or grab it for later usage by virtual machines (&lt;i&gt;Hold&lt;/i&gt;).</source>
        <translation>Задает действие, выполняемое основным ПК при подключении совпадающего устройства: передать его в основную ОС (&lt;i&gt;Игнорировать&lt;/i&gt;) или захватить его для дальнейшего использования виртуальными машинами (&lt;i&gt;Удержать&lt;/i&gt;).</translation>
    </message>
    <message>
        <source>USB Filter Details</source>
        <translation>Свойства USB-фильтра</translation>
    </message>
</context>
<context>
    <name>UIMachineWindowNormal</name>
    <message>
        <source>Shows the currently assigned Host key.&lt;br&gt;This key, when pressed alone, toggles the keyboard and mouse capture state. It can also be used in combination with other keys to quickly perform actions from the main menu.</source>
        <translation>Показывает назначенную хост-клавишу.&lt;br&gt;Эта клавиша, если ее нажимать отдельно, переключает состояние захвата клавиатуры и мыши. Ее можно также использовать в сочетании с другими клавишами для быстрого выполнения действий из главного меню.</translation>
    </message>
</context>
<context>
    <name>UIMediumTypeChangeDialog</name>
    <message>
        <source>Modify medium attributes</source>
        <translation>Изменить атрибуты носителя</translation>
    </message>
    <message>
        <source>&lt;p&gt;You are about to change the attributes of the virtual disk located in &lt;b&gt;%1&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Please choose one of the following medium types and press &lt;b&gt;%2&lt;/b&gt; to proceed or &lt;b&gt;%3&lt;/b&gt; otherwise.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Вы собираетесь изменить атрибуты виртуального носителя, расположенного по адресу &lt;b&gt;%1&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Пожалуйста, выберите один из следующих типов носителя и нажмите &lt;b&gt;%2&lt;/b&gt; чтобы продолжить или &lt;b&gt;%3&lt;/b&gt; в противном случае.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Choose medium type:</source>
        <translation>Выберите тип носителя:</translation>
    </message>
</context>
<context>
    <name>UIMiniProcessWidgetAdditions</name>
    <message>
        <source>Cancel</source>
        <translation>Отмена</translation>
    </message>
    <message>
        <source>Cancel the VirtualBox Guest Additions CD image download</source>
        <translation>Отменить скачивание CD-образа пакета Дополнений гостевой ОС</translation>
    </message>
    <message>
        <source>Downloading the VirtualBox Guest Additions CD image from &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;...&lt;/nobr&gt;</source>
        <translation>Скачивается CD-образ пакета Дополнений гостевой ОС с &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;...&lt;/nobr&gt;</translation>
    </message>
</context>
<context>
    <name>UIMiniProcessWidgetUserManual</name>
    <message>
        <source>Cancel</source>
        <translation>Отмена</translation>
    </message>
    <message>
        <source>Cancel the VirtualBox User Manual download</source>
        <translation>Отменить скачивание Руководства Пользователя</translation>
    </message>
    <message>
        <source>Downloading the VirtualBox User Manual</source>
        <translation>Скачивается Руководство Пользователя</translation>
    </message>
    <message>
        <source>Downloading the VirtualBox User Manual &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;...&lt;/nobr&gt;</source>
        <translation>Скачивается Руководство Пользователя &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;...&lt;/nobr&gt;</translation>
    </message>
</context>
<context>
    <name>UIMultiScreenLayout</name>
    <message>
        <source>Virtual Screen %1</source>
        <translation>Виртуальный экран %1</translation>
    </message>
    <message>
        <source>Use Host Screen %1</source>
        <translation>Использовать дисплей хоста %1</translation>
    </message>
</context>
<context>
    <name>UINewHDWizard</name>
    <message>
        <source>Create New Virtual Disk</source>
        <translation>Создать новый виртуальный диск</translation>
    </message>
    <message>
        <source>Welcome to the Create New Virtual Disk Wizard!</source>
        <translation type="obsolete">Мастер создания нового виртуального диска</translation>
    </message>
    <message>
        <source>Virtual Disk Location and Size</source>
        <translation type="obsolete">Местоположение и размер виртуального диска</translation>
    </message>
    <message>
        <source>Summary</source>
        <translation>Итог</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;%1 Bytes&lt;/nobr&gt;</source>
        <translation type="obsolete">&lt;nobr&gt;%1 байт&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>Hard disk images (*.vdi)</source>
        <translation type="obsolete">Образы жестких дисков (*.vdi)</translation>
    </message>
    <message>
        <source>Select a file for the new hard disk image file</source>
        <translation>Выберите имя файла для хранения нового виртуального диска</translation>
    </message>
    <message>
        <source>&lt; &amp;Back</source>
        <translation type="obsolete">&lt; &amp;Назад</translation>
    </message>
    <message>
        <source>&amp;Next &gt;</source>
        <translation type="obsolete">&amp;Далее &gt;</translation>
    </message>
    <message>
        <source>&amp;Finish</source>
        <translation type="obsolete">&amp;Готово</translation>
    </message>
    <message>
        <source>Type</source>
        <comment>summary</comment>
        <translation type="obsolete">Тип</translation>
    </message>
    <message>
        <source>Location</source>
        <comment>summary</comment>
        <translation>Расположение</translation>
    </message>
    <message>
        <source>Size</source>
        <comment>summary</comment>
        <translation>Размер</translation>
    </message>
    <message>
        <source>Bytes</source>
        <comment>summary</comment>
        <translation type="obsolete">Байт</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation type="obsolete">Отмена</translation>
    </message>
    <message>
        <source>&lt;p&gt;This wizard will help you to create a new virtual hard disk for your virtual machine.&lt;/p&gt;&lt;p&gt;Use the &lt;b&gt;Next&lt;/b&gt; button to go to the next page of the wizard and the &lt;b&gt;Back&lt;/b&gt; button to return to the previous page.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Этот мастер поможет создать новый виртуальный жесткий диск для Вашей виртуальной машины.&lt;/p&gt;&lt;p&gt;Нажмите кнопку &lt;b&gt;Далее&lt;/b&gt;, чтобы перейти к следующей странице мастера, или кнопку &lt;b&gt;Назад&lt;/b&gt; для возврата на предыдущую страницу.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Hard Disk Storage Type</source>
        <translation type="obsolete">Тип виртуального диска</translation>
    </message>
    <message>
        <source>&lt;p&gt;Select the type of virtual hard disk you want to create.&lt;/p&gt;&lt;p&gt;A &lt;b&gt;dynamically expanding storage&lt;/b&gt; initially occupies a very small amount of space on your physical hard disk. It will grow dynamically (up to the size specified) as the Guest OS claims disk space.&lt;/p&gt;&lt;p&gt;A &lt;b&gt;fixed-size storage&lt;/b&gt; does not grow. It is stored in a file of approximately the same size as the size of the virtual hard disk. The creation of a fixed-size storage may take a long time depending on the storage size and the write performance of your harddisk.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Выберите тип виртуального жесткого диска, который Вы хотите создать.&lt;/p&gt;&lt;p&gt;&lt;b&gt;Динамически расширяющийся образ&lt;/b&gt; первоначально занимает очень мало места на физическом жестком диске. Он будет динамически расти (до заданного размера) по мере того, как гостевая ОС использует дисковое пространство.&lt;/p&gt;&lt;p&gt;&lt;b&gt;Образ фиксированного размера&lt;/b&gt; не увеличивается. Он хранится в файле примерно того же размера, что и размер виртуального жесткого диска. Создание жесткого диска фиксированного размера может занять длительное время, в зависимости от размера образа и производительности физического диска.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Storage Type</source>
        <translation type="obsolete">Тип файла</translation>
    </message>
    <message>
        <source>&amp;Dynamically expanding storage</source>
        <translation type="obsolete">Д&amp;инамически расширяющийся образ</translation>
    </message>
    <message>
        <source>&amp;Fixed-size storage</source>
        <translation type="obsolete">Образ &amp;фиксированного размера</translation>
    </message>
    <message>
        <source>&lt;p&gt;Press the &lt;b&gt;Select&lt;/b&gt; button to select the location of a file to store the hard disk data or type a file name in the entry field.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Нажмите кнопку &lt;b&gt;Выбрать&lt;/b&gt; для выбора расположения и имени файла виртуального жесткого диска или введите имя файла в поле ввода.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&amp;Location</source>
        <translation type="obsolete">&amp;Расположение</translation>
    </message>
    <message>
        <source>&lt;p&gt;Select the size of the virtual hard disk in megabytes. This size will be reported to the Guest OS as the maximum size of this hard disk.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Выберите размер виртуального жесткого диска в мегабайтах. Указанный размер будет фигурировать в гостевой ОС в качестве размера данного жесткого диска.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&amp;Size</source>
        <translation type="obsolete">Р&amp;азмер</translation>
    </message>
    <message>
        <source>You are going to create a new virtual hard disk with the following parameters:</source>
        <translation type="obsolete">Вы собираетесь создать виртуальный жесткий диск со следующими параметрами:</translation>
    </message>
    <message>
        <source>If the above settings are correct, press the &lt;b&gt;Finish&lt;/b&gt; button. Once you press it, a new hard disk will be created.</source>
        <translation type="obsolete">Если приведенная выше информация верна, нажмите кнопку &lt;b&gt;Готово&lt;/b&gt;. После этого будет создан новый жесткий диск.</translation>
    </message>
    <message>
        <source>%1_copy</source>
        <comment>copied virtual disk name</comment>
        <translation>%1_копия</translation>
    </message>
    <message>
        <source>Create</source>
        <translation>Создать</translation>
    </message>
    <message>
        <source>Copy Virtual Disk</source>
        <translation>Копировать виртуальный диск</translation>
    </message>
    <message>
        <source>Copy</source>
        <translation>Копировать</translation>
    </message>
    <message>
        <source>Welcome to the virtual disk copying wizard</source>
        <translation>Мастер копирования виртуального диска</translation>
    </message>
    <message>
        <source>&lt;p&gt;This wizard will help you to copy a virtual disk.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Данный мастер поможет Вам создать копию Вашего виртуального диска.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Please select the virtual disk which you would like to copy if it is not already selected. You can either choose one from the list or use the folder icon beside the list to select a virtual disk file.</source>
        <translation>Пожалуйста, выберите виртуальный диск, который Вы желаете скопировать, если он ещё не выбран. Вы можете выбрать его из списка или нажать на кнопку с иконкой папки справа от списка и выбрать файл виртуального диска в открывшемся диалоге.</translation>
    </message>
    <message>
        <source>&amp;VDI (VirtualBox Disk Image)</source>
        <translation>&amp;VDI (VirtualBox Disk Image)</translation>
    </message>
    <message>
        <source>V&amp;MDK (Virtual Machine Disk)</source>
        <translation>V&amp;MDK (Virtual Machine Disk)</translation>
    </message>
    <message>
        <source>V&amp;HD (Virtual Hard Disk)</source>
        <translation>V&amp;HD (Virtual Hard Disk)</translation>
    </message>
    <message>
        <source>Welcome to the virtual disk creation wizard</source>
        <translation>Мастер создания нового виртуального диска</translation>
    </message>
    <message>
        <source>&lt;p&gt;This wizard will help you to create a new virtual disk for your virtual machine.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Данный мастер поможет Вам создать новый виртуальный диск для Вашей виртуальной машины.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Please choose the type of file that you would like to use for the new virtual disk. If you do not need to use it with other virtualization software you can leave this setting unchanged.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Пожалуйста, выберите тип файла, который Вы хотите использовать при создании нового виртуального диска. Если у Вас нет необходимости использовать данный виртуальный диск с другими продуктами программной виртуализации, Вы можете оставить данный параметр как есть.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Virtual disk file type</source>
        <translation>Тип файла виртуального диска</translation>
    </message>
    <message>
        <source>Please choose the type of file that you would like to use for the new virtual disk. If you do not need to use it with other virtualization software you can leave this setting unchanged.</source>
        <translation>Пожалуйста, выберите тип файла, который Вы хотите использовать при создании нового виртуального диска. Если у Вас нет необходимости использовать данный виртуальный диск с другими продуктами программной виртуализации, Вы можете оставить данный параметр как есть.</translation>
    </message>
    <message>
        <source>Virtual disk storage details</source>
        <translation>Дополнительные атрибуты виртуального диска</translation>
    </message>
    <message>
        <source>Please choose whether the new virtual disk file should be allocated as it is used or if it should be created fully allocated.</source>
        <translation>Пожалуйста уточните, должен ли новый виртуальный диск подстраивать свой размер под размер своего содержимого или быть создан сразу заданного размера.</translation>
    </message>
    <message>
        <source>&lt;p&gt;A &lt;b&gt;dynamically allocated&lt;/b&gt; virtual disk file will only use space on your physical hard disk as it fills up, although it will not shrink again automatically when space on it is freed.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Файл &lt;b&gt;динамического&lt;/b&gt; виртуального диска будет занимать необходимое место на Вашем физическом носителе информации лишь по мере заполнения, однако учтите, что он не сможет уменьшиться в размере если место, занятое его содержимым, освободится.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;A &lt;b&gt;fixed size&lt;/b&gt; virtual disk file may take longer to create on some systems but is often faster to use.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Файл &lt;b&gt;фиксированного&lt;/b&gt; виртуального диска может потребовать больше времени при создании на некоторых файловых системах, однако, обычно, он быстрее в использовании.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;You can also choose to &lt;b&gt;split&lt;/b&gt; the virtual disk into several files of up to two gigabytes each. This is mainly useful if you wish to store the virtual machine on removable USB devices or old systems, some of which cannot handle very large files.</source>
        <translation>&lt;p&gt;Вы можете также &lt;b&gt;разделить&lt;/b&gt; виртуальный диск на несколько файлов размером до 2х ГБ. Это может пригодиться если Вы планируете хранить виртуальные носители на съёмных USB носителях или старых файловых системах, некоторые из которых не поддерживают слишком большие файлы.</translation>
    </message>
    <message>
        <source>&amp;Dynamically allocated</source>
        <translation>&amp;Динамический виртуальный диск</translation>
    </message>
    <message>
        <source>&amp;Fixed size</source>
        <translation>&amp;Фиксированный виртуальный диск</translation>
    </message>
    <message>
        <source>&amp;Split into files of less than 2GB</source>
        <translation>&amp;Разделить на файлы размером до 2х ГБ</translation>
    </message>
    <message>
        <source>Virtual disk file location and size</source>
        <translation>Расположение и размер виртуального диска</translation>
    </message>
    <message>
        <source>Select the size of the virtual disk in megabytes. This size will be reported to the Guest OS as the maximum size of this virtual disk.</source>
        <translation>Выберите размер виртуального диска в мегабайтах. Указанный размер будет фигурировать в гостевой ОС в качестве размера данного виртуального диска.</translation>
    </message>
    <message>
        <source>Virtual disk file location</source>
        <translation>Расположение виртуального диска</translation>
    </message>
    <message>
        <source>Please type the name of the new virtual disk file into the box below or click on the folder icon to select a different folder to create the file in.</source>
        <translation>Нажмите кнопку &lt;b&gt;Выбрать&lt;/b&gt; для выбора расположения/имени файла виртуального диска или введите имя файла в поле ввода.</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;%1 (%2 B)&lt;/nobr&gt;</source>
        <translation>&lt;nobr&gt;%1 (%2 Б)&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>You are going to create a new virtual disk with the following parameters:</source>
        <translation>Вы собираетесь создать новый виртуальный диск со следующими параметрами:</translation>
    </message>
    <message>
        <source>You are going to create a copied virtual disk with the following parameters:</source>
        <translation>Вы собираетесь создать копию виртуального диска со следующими параметрами:</translation>
    </message>
    <message>
        <source>If the above settings are correct, press the &lt;b&gt;%1&lt;/b&gt; button. Once you press it the new virtual disk file will be created.</source>
        <translation>Если приведенная выше информация верна, нажмите кнопку &lt;b&gt;%1&lt;/b&gt;. После этого будет создан новый виртуальный диск.</translation>
    </message>
    <message>
        <source>%1 B</source>
        <translation>%1 Б</translation>
    </message>
    <message>
        <source>File type</source>
        <comment>summary</comment>
        <translation>Тип файла</translation>
    </message>
    <message>
        <source>Details</source>
        <comment>summary</comment>
        <translation>Дополнительно</translation>
    </message>
</context>
<context>
    <name>UINewHDWizardPageFormat</name>
    <message>
        <source>File type</source>
        <translation>Тип файла</translation>
    </message>
</context>
<context>
    <name>UINewHDWizardPageOptions</name>
    <message>
        <source>&amp;Location</source>
        <translation>&amp;Расположение</translation>
    </message>
    <message>
        <source>&amp;Size</source>
        <translation>Р&amp;азмер</translation>
    </message>
</context>
<context>
    <name>UINewHDWizardPageVariant</name>
    <message>
        <source>Storage details</source>
        <translation>Дополнительные атрибуты</translation>
    </message>
</context>
<context>
    <name>UINewHDWizardPageWelcome</name>
    <message>
        <source>Welcome to the Create New Virtual Disk Wizard!</source>
        <translation type="obsolete">Мастер создания нового виртуального жёсткого диска</translation>
    </message>
    <message>
        <source>&lt;p&gt;This wizard will help you to create a new virtual hard disk for your virtual machine.&lt;/p&gt;&lt;p&gt;%1&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Данный мастер поможет Вам осуществить создание нового виртуального жёсткого диска для Вашей виртуальной машины.&lt;/p&gt;&lt;p&gt;%1&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Virtual disk to copy</source>
        <translation>Источник копирования</translation>
    </message>
    <message>
        <source>Choose a virtual hard disk file...</source>
        <translation>Выбрать файл виртуального диска...</translation>
    </message>
</context>
<context>
    <name>UINewHDWzdPage2</name>
    <message>
        <source>&lt;p&gt;Select the type of virtual hard disk you want to create.&lt;/p&gt;&lt;p&gt;A &lt;b&gt;dynamically expanding storage&lt;/b&gt; initially occupies a very small amount of space on your physical hard disk. It will grow dynamically (up to the size specified) as the Guest OS claims disk space.&lt;/p&gt;&lt;p&gt;A &lt;b&gt;fixed-size storage&lt;/b&gt; does not grow. It is stored in a file of approximately the same size as the size of the virtual hard disk. The creation of a fixed-size storage may take a long time depending on the storage size and the write performance of your harddisk.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Выберите тип образа виртуального жёсткого диска, который Вы хотите создать.&lt;/p&gt;&lt;p&gt;&lt;b&gt;Динамически расширяющийся образ&lt;/b&gt; первоначально занимает очень мало места на физическом жёстком диске. Он будет динамически расти (до заданного размера) по мере того, как гостевая ОС использует дисковое пространство.&lt;/p&gt;&lt;p&gt;&lt;b&gt;Образ фиксированного размера&lt;/b&gt; не увеличивается. Он хранится в файле примерно того же размера, что и размер виртуального жёсткого диска. Создание жёсткого диска фиксированного размера может занять длительное время, в зависимости от размера образа и производительности физического диска.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Storage Type</source>
        <translation type="obsolete">Тип образа виртуального жёсткого диска</translation>
    </message>
    <message>
        <source>&amp;Dynamically expanding storage</source>
        <translation type="obsolete">Д&amp;инамически расширяющийся образ</translation>
    </message>
    <message>
        <source>&amp;Fixed-size storage</source>
        <translation type="obsolete">Образ &amp;фиксированного размера</translation>
    </message>
    <message>
        <source>Hard Disk Storage Type</source>
        <translation type="obsolete">Тип образа виртуального жёсткого диска</translation>
    </message>
</context>
<context>
    <name>UINewHDWzdPage3</name>
    <message>
        <source>&lt;p&gt;Press the &lt;b&gt;Select&lt;/b&gt; button to select the location of a file to store the hard disk data or type a file name in the entry field.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Нажмите кнопку &lt;b&gt;Выбрать&lt;/b&gt; для выбора расположения и имени файла виртуального жёсткого диска или введите имя файла в поле ввода.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&amp;Location</source>
        <translation type="obsolete">&amp;Расположение</translation>
    </message>
    <message>
        <source>&lt;p&gt;Select the size of the virtual hard disk in megabytes. This size will be reported to the Guest OS as the maximum size of this hard disk.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Выберите размер виртуального жёсткого диска в мегабайтах. Указанный размер будет фигурировать в гостевой ОС в качестве размера данного жёсткого диска.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&amp;Size</source>
        <translation type="obsolete">Р&amp;азмер</translation>
    </message>
    <message>
        <source>Virtual Disk Location and Size</source>
        <translation type="obsolete">Местоположение и размер виртуального жёсткого диска</translation>
    </message>
    <message>
        <source>Select a file for the new hard disk image file</source>
        <translation type="obsolete">Выберите файл для образа нового жёсткого диска</translation>
    </message>
    <message>
        <source>Hard disk images (*.vdi)</source>
        <translation type="obsolete">Образы жёстких дисков (*.vdi)</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;%1 (%2 B)&lt;/nobr&gt;</source>
        <translation type="obsolete">&lt;nobr&gt;%1 (%2 Б)&lt;/nobr&gt;</translation>
    </message>
</context>
<context>
    <name>UINewHDWzdPage4</name>
    <message>
        <source>You are going to create a new virtual hard disk with the following parameters:</source>
        <translation type="obsolete">Вы собираетесь создать виртуальный жёсткий диск со следующими параметрами:</translation>
    </message>
    <message>
        <source>Summary</source>
        <translation type="obsolete">Итог</translation>
    </message>
    <message>
        <source>%1 B</source>
        <translation type="obsolete">%1 Б</translation>
    </message>
    <message>
        <source>Type</source>
        <comment>summary</comment>
        <translation type="obsolete">Тип</translation>
    </message>
    <message>
        <source>Location</source>
        <comment>summary</comment>
        <translation type="obsolete">Расположение</translation>
    </message>
    <message>
        <source>Size</source>
        <comment>summary</comment>
        <translation type="obsolete">Размер</translation>
    </message>
    <message>
        <source>If the above settings are correct, press the &lt;b&gt;%1&lt;/b&gt; button. Once you press it, a new hard disk will be created.</source>
        <translation type="obsolete">Если приведенная выше информация верна, нажмите кнопку &lt;b&gt;%1&lt;/b&gt;. После этого будет создан новый жёсткий диск.</translation>
    </message>
</context>
<context>
    <name>UINewVMWzd</name>
    <message>
        <source>Create New Virtual Machine</source>
        <translation>Создать новую виртуальную машину</translation>
    </message>
    <message>
        <source>Welcome to the New Virtual Machine Wizard!</source>
        <translation type="obsolete">Мастер создания новой виртуальной машины</translation>
    </message>
    <message>
        <source>N&amp;ame</source>
        <translation type="obsolete">&amp;Имя</translation>
    </message>
    <message>
        <source>OS &amp;Type</source>
        <translation type="obsolete">&amp;Тип ОС</translation>
    </message>
    <message>
        <source>VM Name and OS Type</source>
        <translation type="obsolete">Имя машины и тип ОС</translation>
    </message>
    <message>
        <source>&lt;p&gt;Select the amount of base memory (RAM) in megabytes to be allocated to the virtual machine.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Выберите количество основной памяти (RAM) в мегабайтах, выделяемой виртуальной машине.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Base &amp;Memory Size</source>
        <translation type="obsolete">&amp;Размер основной памяти</translation>
    </message>
    <message>
        <source>MB</source>
        <translation type="obsolete">МБ</translation>
    </message>
    <message>
        <source>Memory</source>
        <translation type="obsolete">Память</translation>
    </message>
    <message>
        <source>N&amp;ew...</source>
        <translation type="obsolete">&amp;Создать...</translation>
    </message>
    <message>
        <source>E&amp;xisting...</source>
        <translation type="obsolete">С&amp;уществующий...</translation>
    </message>
    <message>
        <source>Virtual Hard Disk</source>
        <translation type="obsolete">Виртуальный жесткий диск</translation>
    </message>
    <message>
        <source>Summary</source>
        <translation type="obsolete">Итог</translation>
    </message>
    <message>
        <source>The recommended base memory size is &lt;b&gt;%1&lt;/b&gt; MB.</source>
        <translation type="obsolete">Рекомендуемый размер основной памяти: &lt;b&gt;%1&lt;/b&gt; Мб.</translation>
    </message>
    <message>
        <source>The recommended size of the boot hard disk is &lt;b&gt;%1&lt;/b&gt; MB.</source>
        <translation type="obsolete">Рекомендуемый размер загрузочного жесткого диска: &lt;b&gt;%1&lt;/b&gt; Мб.</translation>
    </message>
    <message>
        <source>&lt;p&gt;This wizard will guide you through the steps that are necessary to create a new virtual machine for VirtualBox.&lt;/p&gt;&lt;p&gt;Use the &lt;b&gt;Next&lt;/b&gt; button to go the next page of the wizard and the &lt;b&gt;Back&lt;/b&gt; button to return to the previous page.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Этот мастер поможет Вам выполнить шаги, необходимые для создания новой виртуальной машины для VirtualBox.&lt;/p&gt;&lt;p&gt;Нажмите кнопку &lt;b&gt;Далее&lt;/b&gt;, чтобы перейти к следующей странице мастера, или кнопку &lt;b&gt;Назад&lt;/b&gt; для возврата на предыдущую страницу.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt; &amp;Back</source>
        <translation type="obsolete">&lt; &amp;Назад</translation>
    </message>
    <message>
        <source>&amp;Next &gt;</source>
        <translation type="obsolete">&amp;Далее &gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Enter a name for the new virtual machine and select the type of the guest operating system you plan to install onto the virtual machine.&lt;/p&gt;&lt;p&gt;The name of the virtual machine usually indicates its software and hardware configuration. It will be used by all VirtualBox components to identify your virtual machine.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Введите имя для новой виртуальной машины и выберите тип гостевой операционной системы, которую Вы планируете установить на эту машину.&lt;/p&gt;&lt;p&gt;Имя виртуальной машины обычно отражает ее программную и аппаратную конфигурацию. Это имя будет использоваться всеми приложениями VirtualBox для обозначения созданной виртуальной машины.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;You are going to create a new virtual machine with the following parameters:&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Вы собираетесь создать виртуальную машину со следующими параметрами:&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;If the above is correct press the &lt;b&gt;Finish&lt;/b&gt; button. Once you press it, a new virtual machine will be created. &lt;/p&gt;&lt;p&gt;Note that you can alter these and all other setting of the created virtual machine at any time using the &lt;b&gt;Settings&lt;/b&gt; dialog accessible through the menu of the main window.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Если приведенная выше информация верна, нажмите кнопку &lt;b&gt;Готово&lt;/b&gt;. После этого будет создана новая виртуальная машина. &lt;/p&gt;&lt;p&gt;Обратите внимание, что эти и другие параметры созданной машины можно будет изменить в любое время с помощью диалога &lt;b&gt;Свойства&lt;/b&gt;, доступ к которому можно получить через меню главного окна.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&amp;Finish</source>
        <translation type="obsolete">&amp;Готово</translation>
    </message>
    <message>
        <source>MB</source>
        <comment>megabytes</comment>
        <translation type="obsolete">МБ</translation>
    </message>
    <message>
        <source>Name</source>
        <comment>summary</comment>
        <translation type="obsolete">Имя</translation>
    </message>
    <message>
        <source>OS Type</source>
        <comment>summary</comment>
        <translation type="obsolete">Тип ОС</translation>
    </message>
    <message>
        <source>Base Memory</source>
        <comment>summary</comment>
        <translation type="obsolete">Основная память</translation>
    </message>
    <message>
        <source>Boot Hard Disk</source>
        <comment>summary</comment>
        <translation type="obsolete">Загрузочный жесткий диск</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation type="obsolete">Отмена</translation>
    </message>
    <message>
        <source>&lt;p&gt;Select a hard disk image to be used as the boot hard disk of the virtual machine. You can either create a new hard disk using the &lt;b&gt;New&lt;/b&gt; button or select an existing hard disk image from the drop-down list or by pressing the &lt;b&gt;Existing&lt;/b&gt; button (to invoke the Virtual Media Manager dialog).&lt;/p&gt;&lt;p&gt;If you need a more complicated hard disk setup, you can also skip this step and attach hard disks later using the VM Settings dialog.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Выберите жесткий диск, который будет загрузочным диском виртуальной машины. Вы можете создать новый жесткий диск, нажав кнопку &lt;b&gt;Создать&lt;/b&gt;, либо выбрать существующий из выпадающего списка или из Менеджера виртуальных носителей (который откроется при нажатии на кнопку &lt;b&gt;Существующий&lt;/b&gt;).&lt;/p&gt;&lt;p&gt;Если Вам требуется более сложная конфигурация жестких дисков, то можно пропустить этот шаг и подсоединить жесткие диски позднее с помощью диалога Свойств машины.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Boot Hard &amp;Disk (Primary Master)</source>
        <translation type="obsolete">&amp;Загрузочный жёсткий диск (первичный мастер)</translation>
    </message>
    <message>
        <source>&amp;Create new hard disk</source>
        <translation type="obsolete">&amp;Создать новый жёсткий диск</translation>
    </message>
    <message>
        <source>&amp;Use existing hard disk</source>
        <translation type="obsolete">&amp;Использовать существующий жёсткий диск</translation>
    </message>
    <message>
        <source>Create</source>
        <translation></translation>
    </message>
</context>
<context>
    <name>UINewVMWzdPage1</name>
    <message>
        <source>Welcome to the New Virtual Machine Wizard!</source>
        <translation>Мастер создания новой виртуальной машины</translation>
    </message>
    <message>
        <source>&lt;p&gt;This wizard will guide you through the steps that are necessary to create a new virtual machine for VirtualBox.&lt;/p&gt;&lt;p&gt;%1&lt;/p&gt;</source>
        <translation>&lt;p&gt;Данный мастер поможет Вам осуществить создание новой виртуальной машины VirtualBox.&lt;/p&gt;&lt;p&gt;%1&lt;/p&gt;</translation>
    </message>
</context>
<context>
    <name>UINewVMWzdPage2</name>
    <message>
        <source>&lt;p&gt;Enter a name for the new virtual machine and select the type of the guest operating system you plan to install onto the virtual machine.&lt;/p&gt;&lt;p&gt;The name of the virtual machine usually indicates its software and hardware configuration. It will be used by all VirtualBox components to identify your virtual machine.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Введите имя для новой виртуальной машины и выберите тип гостевой операционной системы, которую Вы планируете установить на эту машину.&lt;/p&gt;&lt;p&gt;Имя виртуальной машины обычно отражает ее программную и аппаратную конфигурацию. Это имя будет использоваться всеми компонентами VirtualBox для обозначения данной виртуальной машины.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>N&amp;ame</source>
        <translation>&amp;Имя</translation>
    </message>
    <message>
        <source>OS &amp;Type</source>
        <translation>&amp;Тип ОС</translation>
    </message>
    <message>
        <source>VM Name and OS Type</source>
        <translation>Имя машины и тип ОС</translation>
    </message>
</context>
<context>
    <name>UINewVMWzdPage3</name>
    <message>
        <source>&lt;p&gt;Select the amount of base memory (RAM) in megabytes to be allocated to the virtual machine.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Выберите количество основной памяти (RAM или ОЗУ) в мегабайтах, выделяемой виртуальной машине.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Base &amp;Memory Size</source>
        <translation>&amp;Размер основной памяти</translation>
    </message>
    <message>
        <source>MB</source>
        <translation>МБ</translation>
    </message>
    <message>
        <source>Memory</source>
        <translation>Память</translation>
    </message>
    <message>
        <source>The recommended base memory size is &lt;b&gt;%1&lt;/b&gt; MB.</source>
        <translation>Рекомендуемый размер основной памяти: &lt;b&gt;%1&lt;/b&gt; МБ.</translation>
    </message>
    <message>
        <source>MB</source>
        <comment>size suffix MBytes=1024 KBytes</comment>
        <translation type="obsolete">МБ</translation>
    </message>
</context>
<context>
    <name>UINewVMWzdPage4</name>
    <message>
        <source>&lt;p&gt;Select a hard disk image to be used as the boot hard disk of the virtual machine. You can either create a new hard disk using the &lt;b&gt;New&lt;/b&gt; button or select an existing hard disk image from the drop-down list or by pressing the &lt;b&gt;Existing&lt;/b&gt; button (to invoke the Virtual Media Manager dialog).&lt;/p&gt;&lt;p&gt;If you need a more complicated hard disk setup, you can also skip this step and attach hard disks later using the VM Settings dialog.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Выберите жёсткий диск, который будет загрузочным диском виртуальной машины. Вы можете создать новый жёсткий диск, выбрав опцию &lt;b&gt;Создать новый жёсткий диск&lt;/b&gt;, либо указать существующий, выбрав опцию &lt;b&gt;Использовать существующий жёсткий диск&lt;/b&gt;, а затем выбрать диск из выпадающего списка или из &lt;b&gt;Менеджера виртуальных носителей&lt;/b&gt; (который откроется при нажатии на кнопку справа от списка).&lt;/p&gt;&lt;p&gt;Если Вам требуется более сложная конфигурация жёстких дисков, то можно пропустить этот шаг и подсоединить жёсткие диски позднее с помощью диалога настроек машины.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Boot Hard &amp;Disk</source>
        <translation>&amp;Загрузочный жёсткий диск</translation>
    </message>
    <message>
        <source>&amp;Create new hard disk</source>
        <translation>&amp;Создать новый жёсткий диск</translation>
    </message>
    <message>
        <source>&amp;Use existing hard disk</source>
        <translation>&amp;Использовать существующий жёсткий диск</translation>
    </message>
    <message>
        <source>Virtual Hard Disk</source>
        <translation>Виртуальный жёсткий диск</translation>
    </message>
    <message>
        <source>The recommended size of the boot hard disk is &lt;b&gt;%1&lt;/b&gt; MB.</source>
        <translation type="obsolete">Рекомендуемый размер загрузочного жёсткого диска: &lt;b&gt;%1&lt;/b&gt; МБ.</translation>
    </message>
    <message>
        <source>&lt;p&gt;Select a virtual hard disk to be used as the boot hard disk of the virtual machine. You can either create a new hard disk or select an existing one from the drop-down list or by pressing corresponding button (to invoke file-open window).&lt;/p&gt;&lt;p&gt;If you need a more complicated hard disk setup, you can also skip this step and attach hard disks later using the VM Settings dialog.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Выберите виртуальный жёсткий диск, который будет загрузочным диском виртуальной машины. Вы можете создать новый жесткий диск либо выбрать существующий, нажав соответствующую кнопку для вызова диалога открытия файла.&lt;/p&gt;&lt;p&gt;Если Вам требуется более сложная конфигурация жестких дисков, то можно пропустить этот шаг и подсоединить жесткие диски позднее с помощью диалога Свойств машины.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Choose a virtual hard disk file...</source>
        <translation>Выбрать образ жёсткого диска...</translation>
    </message>
    <message>
        <source>The recommended size of the boot hard disk is &lt;b&gt;%1&lt;/b&gt;.</source>
        <translation>Рекомендуемый размер загрузочного жёсткого диска: &lt;b&gt;%1&lt;/b&gt; МБ.</translation>
    </message>
</context>
<context>
    <name>UINewVMWzdPage5</name>
    <message>
        <source>&lt;p&gt;You are going to create a new virtual machine with the following parameters:&lt;/p&gt;</source>
        <translation>&lt;p&gt;Вы собираетесь создать виртуальную машину со следующими параметрами:&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Summary</source>
        <translation>Итог</translation>
    </message>
    <message>
        <source>Name</source>
        <comment>summary</comment>
        <translation>Имя</translation>
    </message>
    <message>
        <source>OS Type</source>
        <comment>summary</comment>
        <translation>Тип ОС</translation>
    </message>
    <message>
        <source>Base Memory</source>
        <comment>summary</comment>
        <translation>Основная память</translation>
    </message>
    <message>
        <source>MB</source>
        <comment>size suffix MBytes=1024KBytes</comment>
        <translation type="obsolete">МБ</translation>
    </message>
    <message>
        <source>Boot Hard Disk</source>
        <comment>summary</comment>
        <translation>Загрузочный жёсткий диск</translation>
    </message>
    <message>
        <source>&lt;p&gt;If the above is correct press the &lt;b&gt;%1&lt;/b&gt; button. Once you press it, a new virtual machine will be created. &lt;/p&gt;&lt;p&gt;Note that you can alter these and all other setting of the created virtual machine at any time using the &lt;b&gt;Settings&lt;/b&gt; dialog accessible through the menu of the main window.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Если приведенная выше информация верна, нажмите кнопку &lt;b&gt;%1&lt;/b&gt;. После этого будет создана новая виртуальная машина. &lt;/p&gt;&lt;p&gt;Обратите внимание, что эти и другие параметры созданной машины можно будет изменить в любое время с помощью диалога &lt;b&gt;Свойства&lt;/b&gt;, доступ к которому можно получить через меню главного окна VirtualBox.&lt;/p&gt;</translation>
    </message>
</context>
<context>
    <name>UIPortForwardingModel</name>
    <message>
        <source>Name</source>
        <translation>Имя</translation>
    </message>
    <message>
        <source>Protocol</source>
        <translation>Протокол</translation>
    </message>
    <message>
        <source>Host IP</source>
        <translation>IP хоста</translation>
    </message>
    <message>
        <source>Host Port</source>
        <translation>Порт хоста</translation>
    </message>
    <message>
        <source>Guest IP</source>
        <translation>IP гостя</translation>
    </message>
    <message>
        <source>Guest Port</source>
        <translation>Порт гостя</translation>
    </message>
</context>
<context>
    <name>UIProgressDialog</name>
    <message>
        <source>&amp;Cancel</source>
        <translation>&amp;Отмена</translation>
    </message>
    <message>
        <source>Time remaining: %1</source>
        <translation type="obsolete">Времени до завершения: %1</translation>
    </message>
    <message>
        <source>%1 days, %2 hours remaining</source>
        <translation type="obsolete">Осталось дней: %1, часов: %2</translation>
    </message>
    <message>
        <source>%1 days, %2 minutes remaining</source>
        <translation type="obsolete">Осталось дней: %1, минут: %2</translation>
    </message>
    <message>
        <source>%1 days remaining</source>
        <translation type="obsolete">Осталось дней: %1</translation>
    </message>
    <message>
        <source>1 day, %1 hours remaining</source>
        <translation type="obsolete">Осталось дней: 1, часов: %1</translation>
    </message>
    <message>
        <source>1 day, %1 minutes remaining</source>
        <translation type="obsolete">Осталось дней: 1, минут: %1</translation>
    </message>
    <message>
        <source>1 day remaining</source>
        <translation type="obsolete">Остался 1 день</translation>
    </message>
    <message>
        <source>%1 hours, %2 minutes remaining</source>
        <translation type="obsolete">Осталось часов: %1, минут: %2</translation>
    </message>
    <message>
        <source>1 hour, %1 minutes remaining</source>
        <translation type="obsolete">Осталось часов: 1, минут: %1</translation>
    </message>
    <message>
        <source>1 hour remaining</source>
        <translation type="obsolete">Остался 1 час</translation>
    </message>
    <message>
        <source>%1 minutes remaining</source>
        <translation type="obsolete">Осталось минут: %1</translation>
    </message>
    <message>
        <source>1 minute, %2 seconds remaining</source>
        <translation type="obsolete">Осталось минут: 1, секунд: %2</translation>
    </message>
    <message>
        <source>1 minute remaining</source>
        <translation type="obsolete">Осталась 1 минута</translation>
    </message>
    <message>
        <source>%1 seconds remaining</source>
        <translation type="obsolete">Осталось секунд: %1</translation>
    </message>
    <message>
        <source>A few seconds remaining</source>
        <translation>Осталось несколько секунд</translation>
    </message>
    <message>
        <source>Canceling...</source>
        <translation>Отмена...</translation>
    </message>
    <message>
        <source>Cancel the current operation</source>
        <translation>Отменить текущую операцию</translation>
    </message>
    <message>
        <source>%1, %2 remaining</source>
        <comment>You may wish to translate this more like &quot;Time remaining: %1, %2&quot;</comment>
        <translation>Времени осталось: %1, %2</translation>
    </message>
    <message>
        <source>%1 remaining</source>
        <comment>You may wish to translate this more like &quot;Time remaining: %1&quot;</comment>
        <translation>Времени осталось: %1</translation>
    </message>
</context>
<context>
    <name>UISession</name>
    <message>
        <source>Install</source>
        <translation>Установка</translation>
    </message>
</context>
<context>
    <name>UISettingsDialog</name>
    <message>
        <source>&lt;i&gt;Select a settings category from the list on the left-hand side and move the mouse over a settings item to get more information.&lt;/i&gt;</source>
        <translation>&lt;i&gt;Выберите раздел настроек из списка слева, после чего наведите курсор мыши на нужные элементы настроек для получения подробной информации.&lt;i&gt;</translation>
    </message>
    <message>
        <source>On the &lt;b&gt;%1&lt;/b&gt; page, %2</source>
        <translation>На странице &lt;b&gt;&apos;%1&apos;&lt;/b&gt;: %2</translation>
    </message>
    <message>
        <source>Invalid settings detected</source>
        <translation>Обнаружены неправильные настройки</translation>
    </message>
    <message>
        <source>Non-optimal settings detected</source>
        <translation>Обнаружены неоптимальные настройки</translation>
    </message>
    <message>
        <source>Settings</source>
        <translation>Свойства</translation>
    </message>
</context>
<context>
    <name>UISettingsDialogGlobal</name>
    <message>
        <source>General</source>
        <translation>Общие</translation>
    </message>
    <message>
        <source>Input</source>
        <translation>Ввод</translation>
    </message>
    <message>
        <source>Update</source>
        <translation>Обновления</translation>
    </message>
    <message>
        <source>Language</source>
        <translation>Язык</translation>
    </message>
    <message>
        <source>USB</source>
        <translation>USB</translation>
    </message>
    <message>
        <source>Network</source>
        <translation>Сеть</translation>
    </message>
    <message>
        <source>Extensions</source>
        <translation>Плагины</translation>
    </message>
    <message>
        <source>VirtualBox - %1</source>
        <translation>VirtualBox - %1</translation>
    </message>
    <message>
        <source>Proxy</source>
        <translation>Прокси</translation>
    </message>
</context>
<context>
    <name>UISettingsDialogMachine</name>
    <message>
        <source>General</source>
        <translation>Общие</translation>
    </message>
    <message>
        <source>System</source>
        <translation>Система</translation>
    </message>
    <message>
        <source>Display</source>
        <translation>Дисплей</translation>
    </message>
    <message>
        <source>Storage</source>
        <translation>Носители</translation>
    </message>
    <message>
        <source>Audio</source>
        <translation>Аудио</translation>
    </message>
    <message>
        <source>Network</source>
        <translation>Сеть</translation>
    </message>
    <message>
        <source>Ports</source>
        <translation>Порты</translation>
    </message>
    <message>
        <source>Serial Ports</source>
        <translation>COM-порты</translation>
    </message>
    <message>
        <source>Parallel Ports</source>
        <translation>LPT-порты</translation>
    </message>
    <message>
        <source>USB</source>
        <translation>USB</translation>
    </message>
    <message>
        <source>Shared Folders</source>
        <translation>Общие папки</translation>
    </message>
    <message>
        <source>%1 - %2</source>
        <translation>%1 - %2</translation>
    </message>
    <message>
        <source>you have selected a 64-bit guest OS type for this VM. As such guests require hardware virtualization (VT-x/AMD-V), this feature will be enabled automatically.</source>
        <translation type="obsolete">для этой машины выбран 64-битный тип гостевой ОС. В связи с тем, что такие гостевые ОС требуют активации функций аппаратной виртуализации (VT-x/AMD-V), эти функции будут включены автоматически.</translation>
    </message>
    <message>
        <source>you have 2D Video Acceleration enabled. As 2D Video Acceleration is supported for Windows guests only, this feature will be disabled.</source>
        <translation type="obsolete">для этой машины выбрана функция 2D-ускорения видео. Поскольку данная функция поддерживается лишь классом гостевых систем Windows, она будет отключена.</translation>
    </message>
    <message>
        <source>you have enabled a USB HID (Human Interface Device). This will not work unless USB emulation is also enabled. This will be done automatically when you accept the VM Settings by pressing the OK button.</source>
        <translation type="obsolete">Вы включили поддержку USB HID (устройства пользовательского интерфейса). Данная опция не работает без активированной USB эмуляции, поэтому USB эмуляция будет активирована в момент сохранения настроек виртуальной машины при закрытии данного диалога.</translation>
    </message>
    <message>
        <source>at most one supported</source>
        <translation type="obsolete">поддерживается максимум один</translation>
    </message>
    <message>
        <source>up to %1 supported</source>
        <translation type="obsolete">поддерживается вплоть до %1</translation>
    </message>
    <message>
        <source>you are currently using more storage controllers than a %1 chipset supports. Please change the chipset type on the System settings page or reduce the number of the following storage controllers on the Storage settings page: %2.</source>
        <translation type="obsolete">В данный момент больше контроллеров носителей информации, чем поддерживается чипсетом %1. Пожалуйста, измените тип чипсета на странице &apos;Система&apos; или уменьшите количество следующих контроллеров на странице &apos;Носители&apos;: %2.</translation>
    </message>
</context>
<context>
    <name>UIVMCloseDialog</name>
    <message>
        <source>Close Virtual Machine</source>
        <translation>Закрыть виртуальную машину</translation>
    </message>
    <message>
        <source>You want to:</source>
        <translation>Вы хотите:</translation>
    </message>
    <message>
        <source>&lt;p&gt;Saves the current execution state of the virtual machine to the physical hard disk of the host PC.&lt;/p&gt;&lt;p&gt;Next time this machine is started, it will be restored from the saved state and continue execution from the same place you saved it at, which will let you continue your work immediately.&lt;/p&gt;&lt;p&gt;Note that saving the machine state may take a long time, depending on the guest operating system type and the amount of memory you assigned to the virtual machine.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Сохраняет текущее состояние виртуальной машины на жестком диске основного ПК.&lt;/p&gt;&lt;p&gt;При следующем запуске машина будет восстановлена из этого сохраненного состояния и продолжит выполнение с того места, на котором она была сохранена, позволяя быстро продолжить прерванную работу.&lt;/p&gt;&lt;p&gt;Имейте ввиду, что операция сохранения состояния машины может занять продолжительное время, в зависимости от типа гостевой ОС и размера оперативной памяти, заданного для этой машины.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&amp;Save the machine state</source>
        <translation>&amp;Сохранить состояние машины</translation>
    </message>
    <message>
        <source>&lt;p&gt;Sends the ACPI Power Button press event to the virtual machine.&lt;/p&gt;&lt;p&gt;Normally, the guest operating system running inside the virtual machine will detect this event and perform a clean shutdown procedure. This is a recommended way to turn off the virtual machine because all applications running inside it will get a chance to save their data and state.&lt;/p&gt;&lt;p&gt;If the machine doesn&apos;t respond to this action then the guest operating system may be misconfigured or doesn&apos;t understand ACPI Power Button events at all. In this case you should select the &lt;b&gt;Power off the machine&lt;/b&gt; action to stop virtual machine execution.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Посылает ACPI-сигнал нажатия кнопки питания в виртуальную машину.&lt;/p&gt;&lt;p&gt;Как правило, гостевая операционная система, работающая внутри виртуальной машины, определит этот сигнал и выполнит процедуру нормального завершения работы. Этот вариант является рекомендованным способом выключения виртуальной машины, поскольку в таком случае все работающие приложения гостевой ОC получат возможность сохранить свои данные и состояние.&lt;/p&gt;&lt;p&gt;Если машина никак не реагирует на данное действие, то это значит, что гостевая ОС настроена неправильно, либо она вообще не распознает ACPI-сигнал выключения питания. В таком случае, Вы должны выбрать действие &lt;b&gt;Выключить машину&lt;/b&gt; для остановки виртуальной машины.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>S&amp;end the shutdown signal</source>
        <translation>&amp;Послать сигнал завершения</translation>
    </message>
    <message>
        <source>&lt;p&gt;Turns off the virtual machine.&lt;/p&gt;&lt;p&gt;Note that this action will stop machine execution immediately so that the guest operating system running inside it will not be able to perform a clean shutdown procedure which may result in &lt;i&gt;data loss&lt;/i&gt; inside the virtual machine. Selecting this action is recommended only if the virtual machine does not respond to the &lt;b&gt;Send the shutdown signal&lt;/b&gt; action.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Выключает виртуальную машину.&lt;/p&gt;&lt;p&gt;Имейте ввиду, что это действие приведет к немедленной остановке виртуальной машины. При этом, гостевая операционная система, работающая внутри нее, не получит возможности выполнить процедуру нормального завершения работы, что может привести к &lt;i&gt;потере данных&lt;/i&gt; в работающих внутри машины приложениях. Имеет смысл выбирать этот вариант только в том случае, если виртуальная машина не реагирует на действие &lt;b&gt;Послать сигнал завершения&lt;/b&gt;.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&amp;Power off the machine</source>
        <translation>&amp;Выключить машину</translation>
    </message>
    <message>
        <source>Restore the machine state stored in the current snapshot</source>
        <translation>Восстановить состояние машины, сохранённое в текущем снимке</translation>
    </message>
    <message>
        <source>&lt;p&gt;When checked, the machine will be returned to the state stored in the current snapshot after it is turned off. This is useful if you are sure that you want to discard the results of your last sessions and start again at that snapshot.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Если стоит галочка, то состояние машины будет восстановлено из состояния, сохраненного в текущем снимке, сразу после ее выключения. Это может быть полезным, если Вы уверены, что хотите удалить результаты последнего сеанса работы с виртуальной машиной и вернуться к текущему снимку.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&amp;Restore current snapshot &apos;%1&apos;</source>
        <translation>Восстановить &amp;текущий снимок &apos;%1&apos;</translation>
    </message>
</context>
<context>
    <name>UIVMDesktop</name>
    <message>
        <source>&amp;Details</source>
        <translation>&amp;Детали</translation>
    </message>
    <message>
        <source>&amp;Snapshots</source>
        <translation>&amp;Снимки</translation>
    </message>
</context>
<context>
    <name>UIVMListView</name>
    <message>
        <source>Inaccessible</source>
        <translation>Недоступна</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;%1&lt;br&gt;&lt;/nobr&gt;&lt;nobr&gt;%2 since %3&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;Session %4&lt;/nobr&gt;</source>
        <comment>VM tooltip (name, last state change, session state)</comment>
        <translation>&lt;nobr&gt;%1&lt;br&gt;&lt;/nobr&gt;&lt;nobr&gt;%2 с %3&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;Сессия %4&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;br&gt;&lt;/nobr&gt;&lt;nobr&gt;Inaccessible since %2&lt;/nobr&gt;</source>
        <comment>Inaccessible VM tooltip (name, last state change)</comment>
        <translation>&lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;br&gt;&lt;/nobr&gt;&lt;nobr&gt;Недоступна с %2&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>S&amp;how</source>
        <translation type="obsolete">&amp;Показать</translation>
    </message>
    <message>
        <source>Switch to the window of the selected virtual machine</source>
        <translation type="obsolete">Переключиться в окно выбранной виртуальной машины</translation>
    </message>
    <message>
        <source>S&amp;tart</source>
        <translation type="obsolete">С&amp;тарт</translation>
    </message>
    <message>
        <source>Start the selected virtual machine</source>
        <translation type="obsolete">Начать выполнение выбранной виртуальной машины</translation>
    </message>
    <message>
        <source>R&amp;esume</source>
        <translation type="obsolete">П&amp;родолжить</translation>
    </message>
    <message>
        <source>Resume the execution of the virtual machine</source>
        <translation type="obsolete">Возобновить работу приостановленной виртуальной машины</translation>
    </message>
    <message>
        <source>&amp;Pause</source>
        <translation type="obsolete">Па&amp;уза</translation>
    </message>
    <message>
        <source>Suspend the execution of the virtual machine</source>
        <translation type="obsolete">Приостановить работу виртуальной машины</translation>
    </message>
</context>
<context>
    <name>UIVMPreviewWindow</name>
    <message>
        <source>Update Disabled</source>
        <translation>Выключить обновление</translation>
    </message>
    <message>
        <source>Every 0.5 s</source>
        <translation>Каждые полсекунды</translation>
    </message>
    <message>
        <source>Every 1 s</source>
        <translation>Каждую секунду</translation>
    </message>
    <message>
        <source>Every 2 s</source>
        <translation>Каждые 2 секунды</translation>
    </message>
    <message>
        <source>Every 5 s</source>
        <translation>Каждые 5 секунд</translation>
    </message>
    <message>
        <source>Every 10 s</source>
        <translation>Каждые 10 секунд</translation>
    </message>
    <message>
        <source>No Preview</source>
        <translation>Выключить превью</translation>
    </message>
</context>
<context>
    <name>VBoxAboutDlg</name>
    <message>
        <source>VirtualBox - About</source>
        <translation>VirtualBox - О программе</translation>
    </message>
    <message>
        <source>VirtualBox Graphical User Interface</source>
        <translation>Графический интерфейс VirtualBox</translation>
    </message>
    <message>
        <source>Version %1</source>
        <translation>Версия %1</translation>
    </message>
</context>
<context>
    <name>VBoxAddNIDialog</name>
    <message>
        <source>Add Host Interface</source>
        <translation type="obsolete">Добавить хост-интерфейс</translation>
    </message>
    <message>
        <source>Interface Name</source>
        <translation type="obsolete">Имя интерфейса</translation>
    </message>
    <message>
        <source>Descriptive name of the new network interface</source>
        <translation type="obsolete">Описательное имя нового сетевого интерфейса</translation>
    </message>
    <message>
        <source>&amp;OK</source>
        <translation type="obsolete">&amp;ОК</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation type="obsolete">Отмена</translation>
    </message>
</context>
<context>
    <name>VBoxApplianceEditorWgt</name>
    <message>
        <source>Virtual System %1</source>
        <translation>Виртуальная система %1</translation>
    </message>
    <message>
        <source>Name</source>
        <translation>Имя</translation>
    </message>
    <message>
        <source>Product</source>
        <translation>Продукт</translation>
    </message>
    <message>
        <source>Product-URL</source>
        <translation>Ссылка на продукт</translation>
    </message>
    <message>
        <source>Vendor</source>
        <translation>Поставщик</translation>
    </message>
    <message>
        <source>Vendor-URL</source>
        <translation>Ссылка на поставщика</translation>
    </message>
    <message>
        <source>Version</source>
        <translation>Версия</translation>
    </message>
    <message>
        <source>Description</source>
        <translation>Описание</translation>
    </message>
    <message>
        <source>License</source>
        <translation>Лицензия</translation>
    </message>
    <message>
        <source>Guest OS Type</source>
        <translation>Тип гостевой ОС</translation>
    </message>
    <message>
        <source>CPU</source>
        <translation>Процессор</translation>
    </message>
    <message>
        <source>RAM</source>
        <translation>ОЗУ</translation>
    </message>
    <message>
        <source>Hard Disk Controller (IDE)</source>
        <translation>IDE-контроллер жёсткого диска</translation>
    </message>
    <message>
        <source>Hard Disk Controller (SATA)</source>
        <translation>SATA-контроллер жёсткого диска</translation>
    </message>
    <message>
        <source>Hard Disk Controller (SCSI)</source>
        <translation>SCSI-контроллер жёсткого диска</translation>
    </message>
    <message>
        <source>DVD</source>
        <translation>DVD-привод</translation>
    </message>
    <message>
        <source>Floppy</source>
        <translation>Дисковод</translation>
    </message>
    <message>
        <source>Network Adapter</source>
        <translation>Сетевой адаптер</translation>
    </message>
    <message>
        <source>USB Controller</source>
        <translation>USB-контроллер</translation>
    </message>
    <message>
        <source>Sound Card</source>
        <translation>Звуковая карта</translation>
    </message>
    <message>
        <source>Virtual Disk Image</source>
        <translation>Виртуальный образ диска</translation>
    </message>
    <message>
        <source>Unknown Hardware Item</source>
        <translation>Неизвестный элемент оборудования</translation>
    </message>
    <message>
        <source>MB</source>
        <translation type="obsolete">МБ</translation>
    </message>
    <message>
        <source>&lt;b&gt;Original Value:&lt;/b&gt; %1</source>
        <translation>&lt;b&gt;Начальное значение:&lt;/b&gt; %1</translation>
    </message>
    <message>
        <source>Configuration</source>
        <translation>Конфигурация</translation>
    </message>
    <message>
        <source>Warnings:</source>
        <translation>Предупреждения:</translation>
    </message>
    <message>
        <source>MB</source>
        <comment>size suffix MBytes=1024 KBytes</comment>
        <translation type="obsolete">МБ</translation>
    </message>
    <message>
        <source>MB</source>
        <comment>size suffix MBytes=1024KBytes</comment>
        <translation type="obsolete">МБ</translation>
    </message>
    <message>
        <source>Hard Disk Controller (SAS)</source>
        <translation>SAS-контроллер жёсткого диска</translation>
    </message>
    <message>
        <source>When checked a new unique MAC address will assigned to all configured network cards.</source>
        <translation>Если галочка стоит, всем сетевым адаптерам машины будут назначены новые уникальные MAC адреса.</translation>
    </message>
    <message>
        <source>&amp;Reinitialize the MAC address of all network cards</source>
        <translation>&amp;Сгенерировать новые MAC адреса для всех сетевых адаптеров</translation>
    </message>
</context>
<context>
    <name>VBoxCloseVMDlg</name>
    <message>
        <source>Close Virtual Machine</source>
        <translation type="obsolete">Закрыть виртуальную машину</translation>
    </message>
    <message>
        <source>You want to:</source>
        <translation type="obsolete">Вы хотите:</translation>
    </message>
    <message>
        <source>&amp;Save the machine state</source>
        <translation type="obsolete">&amp;Сохранить состояние машины</translation>
    </message>
    <message>
        <source>&amp;Power off the machine</source>
        <translation type="obsolete">&amp;Выключить машину</translation>
    </message>
    <message>
        <source>&amp;Revert to the current snapshot</source>
        <translation type="obsolete">В&amp;ернуться к текущему снимку</translation>
    </message>
    <message>
        <source>Revert the machine state to the state stored in the current snapshot</source>
        <translation type="obsolete">Вернуть состояние машины к состоянию, сохраненному в текущем снимке</translation>
    </message>
    <message>
        <source>S&amp;end the shutdown signal</source>
        <translation type="obsolete">&amp;Послать сигнал завершения</translation>
    </message>
    <message>
        <source>&lt;p&gt;When checked, the machine will be returned to the state stored in the current snapshot after it is turned off. This is useful if you are sure that you want to discard the results of your last sessions and start again at that snapshot.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Если стоит галочка, то состояние машины будет восстановлено из состояния, сохраненного в текущем снимке, сразу после ее выключения. Это может быть полезным, если Вы уверены, что хотите удалить результаты последнего сеанса работы с виртуальной машиной и вернуться к текущему снимку.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Saves the current execution state of the virtual machine to the physical hard disk of the host PC.&lt;/p&gt;&lt;p&gt;Next time this machine is started, it will be restored from the saved state and continue execution from the same place you saved it at, which will let you continue your work immediately.&lt;/p&gt;&lt;p&gt;Note that saving the machine state may take a long time, depending on the guest operating system type and the amount of memory you assigned to the virtual machine.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Сохраняет текущее состояние виртуальной машины на жестком диске основного ПК.&lt;/p&gt;&lt;p&gt;При следующем запуске машина будет восстановлена из этого сохраненного состояния и продолжит выполнение с того места, на котором она была сохранена, позволяя быстро продолжить прерванную работу.&lt;/p&gt;&lt;p&gt;Имейте ввиду, что операция сохранения состояния машины может занять продолжительное время, в зависимости от типа гостевой ОС и размера оперативной памяти, заданного для этой машины.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Sends the ACPI Power Button press event to the virtual machine.&lt;/p&gt;&lt;p&gt;Normally, the guest operating system running inside the virtual machine will detect this event and perform a clean shutdown procedure. This is a recommended way to turn off the virtual machine because all applications running inside it will get a chance to save their data and state.&lt;/p&gt;&lt;p&gt;If the machine doesn&apos;t respond to this action then the guest operating system may be misconfigured or doesn&apos;t understand ACPI Power Button events at all. In this case you should select the &lt;b&gt;Power off the machine&lt;/b&gt; action to stop virtual machine execution.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Посылает ACPI-сигнал нажатия кнопки питания в виртуальную машину.&lt;/p&gt;&lt;p&gt;Как правило, гостевая операционная система, работающая внутри виртуальной машины, определит этот сигнал и выполнит процедуру нормального завершения работы. Этот вариант является рекомендованным способом выключения виртуальной машины, поскольку в таком случае все работающие приложения гостевой ОC получат возможность сохранить свои данные и состояние.&lt;/p&gt;&lt;p&gt;Если машина никак не реагирует на данное действие, то это значит, что гостевая ОС настроена неправильно, либо она вообще не распознает ACPI-сигнал выключения питания. В таком случае, Вы должны выбрать действие &lt;b&gt;Выключить машину&lt;/b&gt; для остановки виртуальной машины.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Turns off the virtual machine.&lt;/p&gt;&lt;p&gt;Note that this action will stop machine execution immediately so that the guest operating system running inside it will not be able to perform a clean shutdown procedure which may result in &lt;i&gt;data loss&lt;/i&gt; inside the virtual machine. Selecting this action is recommended only if the virtual machine does not respond to the &lt;b&gt;Send the shutdown signal&lt;/b&gt; action.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Выключает виртуальную машину.&lt;/p&gt;&lt;p&gt;Имейте ввиду, что это действие приведет к немедленной остановке виртуальной машины. При этом, гостевая операционная система, работающая внутри нее, не получит возможности выполнить процедуру нормального завершения работы, что может привести к &lt;i&gt;потере данных&lt;/i&gt; в работающих внутри машины приложениях. Имеет смысл выбирать этот вариант только в том случае, если виртуальная машина не реагирует на действие &lt;b&gt;Послать сигнал завершения&lt;/b&gt;.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Restore the machine state stored in the current snapshot</source>
        <translation type="obsolete">Восстановить состояние машины, сохранённое в текущем снимке</translation>
    </message>
    <message>
        <source>&amp;Restore current snapshot &apos;%1&apos;</source>
        <translation type="obsolete">Восстановить &amp;текущий снимок &apos;%1&apos;</translation>
    </message>
</context>
<context>
    <name>VBoxConsoleWnd</name>
    <message>
        <source>VirtualBox OSE</source>
        <translation type="obsolete">VirtualBox OSE</translation>
    </message>
    <message>
        <source>&amp;Fullscreen Mode</source>
        <translation type="obsolete">&amp;Полноэкранный режим</translation>
    </message>
    <message>
        <source>Switch to fullscreen mode</source>
        <translation type="obsolete">Переключиться в полноэкранный режим</translation>
    </message>
    <message>
        <source>Mouse Integration</source>
        <comment>enable/disable...</comment>
        <translation type="obsolete">Интеграция мыши</translation>
    </message>
    <message>
        <source>Auto-resize Guest Display</source>
        <comment>enable/disable...</comment>
        <translation type="obsolete">Авто-размер экрана гостевой ОС</translation>
    </message>
    <message>
        <source>Auto-resize &amp;Guest Display</source>
        <translation type="obsolete">Авто-размер экрана &amp;гостевой ОС</translation>
    </message>
    <message>
        <source>Automatically resize the guest display when the window is resized (requires Guest Additions)</source>
        <translation type="obsolete">Автоматически изменять размер экрана гостевой ОС при изменении размеров окна (требуются Дополнения гостевой ОС)</translation>
    </message>
    <message>
        <source>&amp;Adjust Window Size</source>
        <translation type="obsolete">П&amp;одогнать размер окна</translation>
    </message>
    <message>
        <source>Adjust window size and position to best fit the guest display</source>
        <translation type="obsolete">Подогнать размер и положение окна под размер экрана гостевой ОС</translation>
    </message>
    <message>
        <source>&amp;Insert Ctrl-Alt-Del</source>
        <translation type="obsolete">Посл&amp;ать Ctrl-Alt-Del</translation>
    </message>
    <message>
        <source>Send the Ctrl-Alt-Del sequence to the virtual machine</source>
        <translation type="obsolete">Послать последовательность клавиш Ctrl-Alt-Del в виртуальную машину</translation>
    </message>
    <message>
        <source>&amp;Insert Ctrl-Alt-Backspace</source>
        <translation type="obsolete">Посла&amp;ть Ctrl-Alt-Backspace</translation>
    </message>
    <message>
        <source>Send the Ctrl-Alt-Backspace sequence to the virtual machine</source>
        <translation type="obsolete">Послать последовательность клавиш Ctrl-Alt-Backspace в виртуальную машину</translation>
    </message>
    <message>
        <source>&amp;Reset</source>
        <translation type="obsolete">С&amp;брос</translation>
    </message>
    <message>
        <source>Reset the virtual machine</source>
        <translation type="obsolete">Послать сигнал Сброс для перезапуска виртуальной машины</translation>
    </message>
    <message>
        <source>ACPI S&amp;hutdown</source>
        <translation type="obsolete">В&amp;ыключить через ACPI</translation>
    </message>
    <message>
        <source>Send the ACPI Power Button press event to the virtual machine</source>
        <translation type="obsolete">Послать ACPI-сигнал нажатия кнопки питания в виртуальную машину</translation>
    </message>
    <message>
        <source>&amp;Close...</source>
        <translation type="obsolete">&amp;Закрыть...</translation>
    </message>
    <message>
        <source>Close the virtual machine</source>
        <translation type="obsolete">Закрыть виртуальную машину</translation>
    </message>
    <message>
        <source>Take &amp;Snapshot...</source>
        <translation type="obsolete">Сделать с&amp;нимок...</translation>
    </message>
    <message>
        <source>Take a snapshot of the virtual machine</source>
        <translation type="obsolete">Сделать снимок текущего состояния виртуальной машины</translation>
    </message>
    <message>
        <source>&amp;Floppy Image...</source>
        <translation type="obsolete">Образ &amp;дискеты...</translation>
    </message>
    <message>
        <source>Mount a floppy image file</source>
        <translation type="obsolete">Подключить образ дискеты из файла</translation>
    </message>
    <message>
        <source>Unmount F&amp;loppy</source>
        <translation type="obsolete">Отключить д&amp;искету</translation>
    </message>
    <message>
        <source>Unmount the currently mounted floppy media</source>
        <translation type="obsolete">Отключить подключенную дискету или привод</translation>
    </message>
    <message>
        <source>&amp;CD/DVD-ROM Image...</source>
        <translation type="obsolete">О&amp;браз CD/DVD-ROM...</translation>
    </message>
    <message>
        <source>Mount a CD/DVD-ROM image file</source>
        <translation type="obsolete">Подключить образ CD/DVD-ROM из файла</translation>
    </message>
    <message>
        <source>Unmount C&amp;D/DVD-ROM</source>
        <translation type="obsolete">&amp;Отключить CD/DVD-ROM</translation>
    </message>
    <message>
        <source>Unmount the currently mounted CD/DVD-ROM media</source>
        <translation type="obsolete">Отключить подключенное устройство CD/DVD-ROM</translation>
    </message>
    <message>
        <source>Remote Dis&amp;play</source>
        <translation type="obsolete">Уд&amp;аленный дисплей</translation>
    </message>
    <message>
        <source>Enable or disable remote desktop (RDP) connections to this machine</source>
        <translation type="obsolete">Разрешить или запретить подключение удаленных клиентов по протоколу RDP к этой машине </translation>
    </message>
    <message>
        <source>&amp;Shared Folders...</source>
        <translation type="obsolete">О&amp;бщие папки...</translation>
    </message>
    <message>
        <source>Create or modify shared folders</source>
        <translation type="obsolete">Открыть диалог для настройки общих папок</translation>
    </message>
    <message>
        <source>&amp;Install Guest Additions...</source>
        <translation type="obsolete">Ус&amp;тановить Дополнения гостевой ОС...</translation>
    </message>
    <message>
        <source>Mount the Guest Additions installation image</source>
        <translation type="obsolete">Подключить установочный образ CD c пакетом Дополнений гостевой ОС</translation>
    </message>
    <message>
        <source>Mount &amp;Floppy</source>
        <translation type="obsolete">Подключить &amp;дискету</translation>
    </message>
    <message>
        <source>Mount &amp;CD/DVD-ROM</source>
        <translation type="obsolete">&amp;Подключить CD/DVD-ROM</translation>
    </message>
    <message>
        <source>&amp;USB Devices</source>
        <translation type="obsolete">&amp;Устройства USB</translation>
    </message>
    <message>
        <source>&amp;Devices</source>
        <translation type="obsolete">&amp;Устройства</translation>
    </message>
    <message>
        <source>De&amp;bug</source>
        <translation type="obsolete">От&amp;ладка</translation>
    </message>
    <message>
        <source>&amp;Help</source>
        <translation type="obsolete">Справк&amp;а</translation>
    </message>
    <message>
        <source>&lt;hr&gt;The VRDP Server is listening on port %1</source>
        <translation type="obsolete">&lt;hr&gt;VRDP-сервер ожидает соединений на порту %1</translation>
    </message>
    <message>
        <source>&amp;Pause</source>
        <translation type="obsolete">Па&amp;уза</translation>
    </message>
    <message>
        <source>Suspend the execution of the virtual machine</source>
        <translation type="obsolete">Приостановить работу виртуальной машины</translation>
    </message>
    <message>
        <source>R&amp;esume</source>
        <translation type="obsolete">П&amp;родолжить</translation>
    </message>
    <message>
        <source>Resume the execution of the virtual machine</source>
        <translation type="obsolete">Возобновить работу приостановленной виртуальной машины</translation>
    </message>
    <message>
        <source>Disable &amp;Mouse Integration</source>
        <translation type="obsolete">Выключить интеграцию &amp;мыши</translation>
    </message>
    <message>
        <source>Temporarily disable host mouse pointer integration</source>
        <translation type="obsolete">Временно отключить интеграцию указателя мыши</translation>
    </message>
    <message>
        <source>Enable &amp;Mouse Integration</source>
        <translation type="obsolete">Включить интеграцию &amp;мыши</translation>
    </message>
    <message>
        <source>Enable temporarily disabled host mouse pointer integration</source>
        <translation type="obsolete">Включить временно отключенную интеграцию указателя мыши</translation>
    </message>
    <message>
        <source>Snapshot %1</source>
        <translation type="obsolete">Снимок %1</translation>
    </message>
    <message>
        <source>Host Drive </source>
        <translation type="obsolete">Физический привод </translation>
    </message>
    <message>
        <source>&amp;Machine</source>
        <translation type="obsolete">&amp;Машина</translation>
    </message>
    <message>
        <source>&amp;Network Adapters</source>
        <translation type="obsolete">&amp;Сетевые адаптеры</translation>
    </message>
    <message>
        <source>Adapter %1</source>
        <comment>network</comment>
        <translation type="obsolete">Адаптер %1</translation>
    </message>
    <message>
        <source>Mount the selected physical drive of the host PC</source>
        <comment>Floppy tip</comment>
        <translation type="obsolete">Подключить выбранный физический привод основного ПК</translation>
    </message>
    <message>
        <source>Mount the selected physical drive of the host PC</source>
        <comment>CD/DVD tip</comment>
        <translation type="obsolete">Подключить выбранный физический привод основного ПК</translation>
    </message>
    <message>
        <source>Disconnect the cable from the selected virtual network adapter</source>
        <translation type="obsolete">Отключить кабель от виртуального сетевого адаптера</translation>
    </message>
    <message>
        <source>Connect the cable to the selected virtual network adapter</source>
        <translation type="obsolete">Подключить кабель к виртуальному сетевому адаптеру</translation>
    </message>
    <message>
        <source>Seam&amp;less Mode</source>
        <translation type="obsolete">Режим интеграции &amp;дисплея</translation>
    </message>
    <message>
        <source>Switch to seamless desktop integration mode</source>
        <translation type="obsolete">Переключиться в режим интеграции дисплея с рабочим столом</translation>
    </message>
    <message>
        <source>&lt;qt&gt;&lt;nobr&gt;Indicates the activity of the floppy media:&lt;/nobr&gt;%1&lt;/qt&gt;</source>
        <comment>Floppy tooltip</comment>
        <translation type="obsolete">&lt;qt&gt;&lt;nobr&gt;Показывает активность дисковода гибких дисков:&lt;/nobr&gt;%1&lt;/qt&gt;</translation>
    </message>
    <message>
        <source>&lt;br&gt;&lt;nobr&gt;&lt;b&gt;Host Drive&lt;/b&gt;: %1&lt;/nobr&gt;</source>
        <comment>Floppy tooltip</comment>
        <translation type="obsolete">&lt;br&gt;&lt;nobr&gt;&lt;b&gt;Физический привод&lt;/b&gt;: %1&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;br&gt;&lt;nobr&gt;&lt;b&gt;Image&lt;/b&gt;: %1&lt;/nobr&gt;</source>
        <comment>Floppy tooltip</comment>
        <translation type="obsolete">&lt;br&gt;&lt;nobr&gt;&lt;b&gt;Образ&lt;/b&gt;: %1&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;br&gt;&lt;nobr&gt;&lt;b&gt;No media mounted&lt;/b&gt;&lt;/nobr&gt;</source>
        <comment>Floppy tooltip</comment>
        <translation type="obsolete">&lt;br&gt;&lt;nobr&gt;&lt;b&gt;Носители не подключены&lt;/b&gt;&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;qt&gt;&lt;nobr&gt;Indicates the activity of the CD/DVD-ROM media:&lt;/nobr&gt;%1&lt;/qt&gt;</source>
        <comment>DVD-ROM tooltip</comment>
        <translation type="obsolete">&lt;qt&gt;&lt;nobr&gt;Показывает активность дисковода CD/DVD-ROM:&lt;/nobr&gt;%1&lt;/qt&gt;</translation>
    </message>
    <message>
        <source>&lt;br&gt;&lt;nobr&gt;&lt;b&gt;Host Drive&lt;/b&gt;: %1&lt;/nobr&gt;</source>
        <comment>DVD-ROM tooltip</comment>
        <translation type="obsolete">&lt;br&gt;&lt;nobr&gt;&lt;b&gt;Физический привод&lt;/b&gt;: %1&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;br&gt;&lt;nobr&gt;&lt;b&gt;Image&lt;/b&gt;: %1&lt;/nobr&gt;</source>
        <comment>DVD-ROM tooltip</comment>
        <translation type="obsolete">&lt;br&gt;&lt;nobr&gt;&lt;b&gt;Образ&lt;/b&gt;: %1&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;br&gt;&lt;nobr&gt;&lt;b&gt;No media mounted&lt;/b&gt;&lt;/nobr&gt;</source>
        <comment>DVD-ROM tooltip</comment>
        <translation type="obsolete">&lt;br&gt;&lt;nobr&gt;&lt;b&gt;Носители не подключены&lt;/b&gt;&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;qt&gt;&lt;nobr&gt;Indicates activity on the the virtual hard disks:&lt;/nobr&gt;%1&lt;/qt&gt;</source>
        <comment>HDD tooltip</comment>
        <translation type="obsolete">&lt;qt&gt;&lt;nobr&gt;Показывает активность виртуальных жестких дисков:&lt;/nobr&gt;%1&lt;/qt&gt;</translation>
    </message>
    <message>
        <source>&lt;br&gt;&lt;nobr&gt;&lt;b&gt;No hard disks attached&lt;/b&gt;&lt;/nobr&gt;</source>
        <comment>HDD tooltip</comment>
        <translation type="obsolete">&lt;br&gt;&lt;nobr&gt;&lt;b&gt;Жесткие диски не подсоединены&lt;/b&gt;&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;qt&gt;&lt;nobr&gt;Indicates the activity of the network interfaces:&lt;/nobr&gt;%1&lt;/qt&gt;</source>
        <comment>Network adapters tooltip</comment>
        <translation type="obsolete">&lt;qt&gt;&lt;nobr&gt;Показывает активность сетевых интерфейсов:&lt;/nobr&gt;%1&lt;/qt&gt;</translation>
    </message>
    <message>
        <source>&lt;br&gt;&lt;nobr&gt;&lt;b&gt;Adapter %1 (%2)&lt;/b&gt;: cable %3&lt;/nobr&gt;</source>
        <comment>Network adapters tooltip</comment>
        <translation type="obsolete">&lt;br&gt;&lt;nobr&gt;&lt;b&gt;Адаптер %1 (%2)&lt;/b&gt;: кабель %3&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>connected</source>
        <comment>Network adapters tooltip</comment>
        <translation type="obsolete">подключен</translation>
    </message>
    <message>
        <source>disconnected</source>
        <comment>Network adapters tooltip</comment>
        <translation type="obsolete">отключен</translation>
    </message>
    <message>
        <source>&lt;br&gt;&lt;nobr&gt;&lt;b&gt;All network adapters are disabled&lt;/b&gt;&lt;/nobr&gt;</source>
        <comment>Network adapters tooltip</comment>
        <translation type="obsolete">&lt;br&gt;&lt;nobr&gt;&lt;b&gt;Все сетевые адапреты выключены&lt;/b&gt;&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;qt&gt;&lt;nobr&gt;Indicates the activity of the attached USB devices:&lt;/nobr&gt;%1&lt;/qt&gt;</source>
        <comment>USB device tooltip</comment>
        <translation type="obsolete">&lt;qt&gt;&lt;nobr&gt;Показыавет активность подсоединенных USB-устройств:&lt;/nobr&gt;%1&lt;/qt&gt;</translation>
    </message>
    <message>
        <source>&lt;br&gt;&lt;nobr&gt;&lt;b&gt;No USB devices attached&lt;/b&gt;&lt;/nobr&gt;</source>
        <comment>USB device tooltip</comment>
        <translation type="obsolete">&lt;br&gt;&lt;nobr&gt;&lt;b&gt;USB-устройства не подсоединены&lt;/b&gt;&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;br&gt;&lt;nobr&gt;&lt;b&gt;USB Controller is disabled&lt;/b&gt;&lt;/nobr&gt;</source>
        <comment>USB device tooltip</comment>
        <translation type="obsolete">&lt;br&gt;&lt;nobr&gt;&lt;b&gt;Контроллер USB выключен&lt;/b&gt;&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;qt&gt;&lt;nobr&gt;Indicates the activity of the machineof the machine&apos;sapos;s shared folders: shared folders:&lt;/nobr&gt;%1&lt;/qt&gt;</source>
        <comment>Shared folders tooltip</comment>
        <translation type="obsolete">&lt;qt&gt;&lt;nobr&gt;Показывает активность общих папок:&lt;/nobr&gt;%1&lt;/qt&gt;</translation>
    </message>
    <message>
        <source>&lt;br&gt;&lt;nobr&gt;&lt;b&gt;No shared folders&lt;/b&gt;&lt;/nobr&gt;</source>
        <comment>Shared folders tooltip</comment>
        <translation type="obsolete">&lt;br&gt;&lt;nobr&gt;&lt;b&gt;Нет общих папок&lt;/b&gt;&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>Session I&amp;nformation Dialog</source>
        <translation type="obsolete">&amp;Информация о сессии</translation>
    </message>
    <message>
        <source>Show Session Information Dialog</source>
        <translation type="obsolete">Показать диалог с информацией о сессии</translation>
    </message>
    <message>
        <source>&amp;Statistics...</source>
        <comment>debug action</comment>
        <translation type="obsolete">&amp;Статистика...</translation>
    </message>
    <message>
        <source>&amp;Command Line...</source>
        <comment>debug action</comment>
        <translation type="obsolete">&amp;Командная строка...</translation>
    </message>
    <message>
        <source>Indicates whether the guest display auto-resize function is On (&lt;img src=:/auto_resize_on_16px.png/&gt;) or Off (&lt;img src=:/auto_resize_off_16px.png/&gt;). Note that this function requires Guest Additions to be installed in the guest OS.</source>
        <translation type="obsolete">Показывает, что функция авто-изменения размера экрана гостевой ОС включена (&lt;img src=:/auto_resize_on_16px.png/&gt;) или выключена (&lt;img src=:/auto_resize_off_16px.png/&gt;). Обратите внимание, что для этой функции требуется установка Дополнений гостевой ОС.</translation>
    </message>
    <message>
        <source>Indicates whether the host mouse pointer is captured by the guest OS:&lt;br&gt;&lt;nobr&gt;&lt;img src=:/mouse_disabled_16px.png/&gt;&amp;nbsp;&amp;nbsp;pointer is not captured&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;&lt;img src=:/mouse_16px.png/&gt;&amp;nbsp;&amp;nbsp;pointer is captured&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;&lt;img src=:/mouse_seamless_16px.png/&gt;&amp;nbsp;&amp;nbsp;mouse integration (MI) is On&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;&lt;img src=:/mouse_can_seamless_16px.png/&gt;&amp;nbsp;&amp;nbsp;MI is Off, pointer is captured&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;&lt;img src=:/mouse_can_seamless_uncaptured_16px.png/&gt;&amp;nbsp;&amp;nbsp;MI is Off, pointer is not captured&lt;/nobr&gt;&lt;br&gt;Note that the mouse integration feature requires Guest Additions to be installed in the guest OS.</source>
        <translation type="obsolete">Показывает, захвачен ли указатель мыши основного ПК в гостевой ОС:&lt;br&gt;&lt;nobr&gt;&lt;img src=:/mouse_disabled_16px.png/&gt;&amp;nbsp;&amp;nbsp;указатель не захвачен&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;&lt;img src=:/mouse_16px.png/&gt;&amp;nbsp;&amp;nbsp;указатель захвачен&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;&lt;img src=:/mouse_seamless_16px.png/&gt;&amp;nbsp;&amp;nbsp;интеграция мыши (ИМ) включена&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;&lt;img src=:/mouse_can_seamless_16px.png/&gt;&amp;nbsp;&amp;nbsp;ИМ выключена, указатель захвачен&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;&lt;img src=:/mouse_can_seamless_uncaptured_16px.png/&gt;&amp;nbsp;&amp;nbsp;ИМ выключена, указатель не захвачен&lt;/nobr&gt;&lt;br&gt;Обратите внимание, что для интеграции мыши требуется установка Дополнений гостевой ОС.</translation>
    </message>
    <message>
        <source>Indicates whether the keyboard is captured by the guest OS (&lt;img src=:/hostkey_captured_16px.png/&gt;) or not (&lt;img src=:/hostkey_16px.png/&gt;).</source>
        <translation type="obsolete">Показывает, захвачена клавиатура в гостевой ОС (&lt;img src=:/hostkey_captured_16px.png/&gt;) или нет (&lt;img src=:/hostkey_16px.png/&gt;).</translation>
    </message>
    <message>
        <source>Indicates whether the Remote Display (VRDP Server) is enabled (&lt;img src=:/vrdp_16px.png/&gt;) or not (&lt;img src=:/vrdp_disabled_16px.png/&gt;).</source>
        <translation type="obsolete">Показывает, включен удаленный дисплей (VRDP-сервер) (&lt;img src=:/vrdp_16px.png/&gt;) или нет (&lt;img src=:/vrdp_disabled_16px.png/&gt;).</translation>
    </message>
    <message>
        <source>&amp;Logging...</source>
        <comment>debug action</comment>
        <translation type="obsolete">С&amp;бор данных...</translation>
    </message>
    <message>
        <source>Shows the currently assigned Host key.&lt;br&gt;This key, when pressed alone, toggles the keyboard and mouse capture state. It can also be used in combination with other keys to quickly perform actions from the main menu.</source>
        <translation type="obsolete">Показывает назначенную хост-клавишу.&lt;br&gt;Эта клавиша, если ее нажимать отдельно, переключает состояние захвата клавиатуры и мыши. Ее можно также использовать в сочетании с другими клавишами для быстрого выполнения действий из главного меню.</translation>
    </message>
    <message>
        <source>Sun VirtualBox</source>
        <translation type="obsolete">Sun VirtualBox</translation>
    </message>
    <message>
        <source>&lt;qt&gt;Indicates the status of the hardware virtualization features used by this virtual machine:&lt;br&gt;&lt;nobr&gt;&lt;b&gt;%1:&lt;/b&gt;&amp;nbsp;%2&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;&lt;b&gt;%3:&lt;/b&gt;&amp;nbsp;%4&lt;/nobr&gt;&lt;/qt&gt;</source>
        <translation type="obsolete">&lt;qt&gt;Показывает статус опций аппаратной виртуализации используемых виртуальной машиной:&lt;br&gt;&lt;nobr&gt;&lt;b&gt;%1:&lt;/b&gt;&amp;nbsp;%2&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;&lt;b&gt;%3:&lt;/b&gt;&amp;nbsp;%4&lt;/nobr&gt;&lt;/qt&gt;</translation>
    </message>
    <message>
        <source>Indicates the status of the hardware virtualization features used by this virtual machine:&lt;br&gt;&lt;nobr&gt;&lt;b&gt;%1:&lt;/b&gt;&amp;nbsp;%2&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;&lt;b&gt;%3:&lt;/b&gt;&amp;nbsp;%4&lt;/nobr&gt;</source>
        <comment>Virtualization Stuff LED</comment>
        <translation type="obsolete">Показывает статус опций аппаратной виртуализации используемых виртуальной машиной:&lt;br&gt;&lt;nobr&gt;&lt;b&gt;%1:&lt;/b&gt;&amp;nbsp;%2&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;&lt;b&gt;%3:&lt;/b&gt;&amp;nbsp;%4&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;br&gt;&lt;nobr&gt;&lt;b&gt;%1:&lt;/b&gt;&amp;nbsp;%2&lt;/nobr&gt;</source>
        <comment>Virtualization Stuff LED</comment>
        <translation type="obsolete">&lt;br&gt;&lt;nobr&gt;&lt;b&gt;%1:&lt;/b&gt;&amp;nbsp;%2&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source> EXPERIMENTAL build %1r%2 - %3</source>
        <translation type="obsolete">ЭКСПЕРИМЕНТАЛЬНАЯ версия %1r%2 - %3</translation>
    </message>
    <message>
        <source>&amp;CD/DVD Devices</source>
        <translation type="obsolete">&amp;Приводы оптических дисков</translation>
    </message>
    <message>
        <source>&amp;Floppy Devices</source>
        <translation type="obsolete">П&amp;риводы гибких дисков</translation>
    </message>
    <message>
        <source>&amp;Network Adapters...</source>
        <translation type="obsolete">&amp;Сетевые адаптеры...</translation>
    </message>
    <message>
        <source>Change the settings of network adapters</source>
        <translation type="obsolete">Открыть диалог для настройки сетевых адаптеров</translation>
    </message>
    <message>
        <source>&amp;Remote Display</source>
        <translation type="obsolete">У&amp;даленный дисплей</translation>
    </message>
    <message>
        <source>Remote Desktop (RDP) Server</source>
        <comment>enable/disable...</comment>
        <translation type="obsolete">Сервер удаленного дисплея (RDP)</translation>
    </message>
    <message>
        <source>More CD/DVD Images...</source>
        <translation type="obsolete">Прочие образы оптических дисков...</translation>
    </message>
    <message>
        <source>Unmount CD/DVD Device</source>
        <translation type="obsolete">Извлечь образ оптического диска</translation>
    </message>
    <message>
        <source>More Floppy Images...</source>
        <translation type="obsolete">Прочие образы гибких дисков...</translation>
    </message>
    <message>
        <source>Unmount Floppy Device</source>
        <translation type="obsolete">Извлечь образ гибкого диска</translation>
    </message>
    <message>
        <source>No CD/DVD Devices Attached</source>
        <translation type="obsolete">Нет подсоединенных приводов оптических дисков</translation>
    </message>
    <message>
        <source>No Floppy Devices Attached</source>
        <translation type="obsolete">Нет подсоединенных приводов гибких дисков</translation>
    </message>
    <message>
        <source>&lt;p style=&apos;white-space:pre&apos;&gt;&lt;nobr&gt;Indicates the activity of the virtual hard disks:&lt;/nobr&gt;%1&lt;/p&gt;</source>
        <comment>HDD tooltip</comment>
        <translation type="obsolete">&lt;p style=&apos;white-space:pre&apos;&gt;&lt;nobr&gt;Отображает активность виртуальных жёстких дисков:&lt;/nobr&gt;%1&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p style=&apos;white-space:pre&apos;&gt;&lt;nobr&gt;Indicates the activity of the CD/DVD devices:&lt;/nobr&gt;%1&lt;/p&gt;</source>
        <comment>CD/DVD tooltip</comment>
        <translation type="obsolete">&lt;p style=&apos;white-space:pre&apos;&gt;&lt;nobr&gt;Отображает активность приводов оптических дисков:&lt;/nobr&gt;%1&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;br&gt;&lt;nobr&gt;&lt;b&gt;No CD/DVD devices attached&lt;/b&gt;&lt;/nobr&gt;</source>
        <comment>CD/DVD tooltip</comment>
        <translation type="obsolete">&lt;br&gt;&lt;nobr&gt;&lt;b&gt;Нет подсоединенных приводов оптических дисков&lt;/b&gt;&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;p style=&apos;white-space:pre&apos;&gt;&lt;nobr&gt;Indicates the activity of the floppy devices:&lt;/nobr&gt;%1&lt;/p&gt;</source>
        <comment>FD tooltip</comment>
        <translation type="obsolete">&lt;p style=&apos;white-space:pre&apos;&gt;&lt;nobr&gt;Отображает активность приводов гибких дисков:&lt;/nobr&gt;%1&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;br&gt;&lt;nobr&gt;&lt;b&gt;No floppy devices attached&lt;/b&gt;&lt;/nobr&gt;</source>
        <comment>FD tooltip</comment>
        <translation type="obsolete">&lt;br&gt;&lt;nobr&gt;&lt;b&gt;Нет подсоединенных приводов гибких дисков&lt;/b&gt;&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;p style=&apos;white-space:pre&apos;&gt;&lt;nobr&gt;Indicates the activity of the network interfaces:&lt;/nobr&gt;%1&lt;/p&gt;</source>
        <comment>Network adapters tooltip</comment>
        <translation type="obsolete">&lt;p style=&apos;white-space:pre&apos;&gt;&lt;nobr&gt;Отображает активность сетевых адаптеров:&lt;/nobr&gt;%1&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p style=&apos;white-space:pre&apos;&gt;&lt;nobr&gt;Indicates the activity of the attached USB devices:&lt;/nobr&gt;%1&lt;/p&gt;</source>
        <comment>USB device tooltip</comment>
        <translation type="obsolete">&lt;p style=&apos;white-space:pre&apos;&gt;&lt;nobr&gt;Отображает активность подсоединенных USB устройств:&lt;/nobr&gt;%1&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p style=&apos;white-space:pre&apos;&gt;&lt;nobr&gt;Indicates the activity of the machine&apos;s shared folders:&lt;/nobr&gt;%1&lt;/p&gt;</source>
        <comment>Shared folders tooltip</comment>
        <translation type="obsolete">&lt;p style=&apos;white-space:pre&apos;&gt;&lt;nobr&gt;Отображает активность общих папок машины:&lt;/nobr&gt;%1&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Dock Icon</source>
        <translation type="obsolete">Иконка дока</translation>
    </message>
    <message>
        <source>Show Application Icon</source>
        <translation type="obsolete">Показать иконку приложения</translation>
    </message>
    <message>
        <source>Show Monitor Preview</source>
        <translation type="obsolete">Предпросмотр монитора</translation>
    </message>
    <message>
        <source>&lt;br&gt;&lt;nobr&gt;&lt;b&gt;Adapter %1 (%2)&lt;/b&gt;: %3 cable %4&lt;/nobr&gt;</source>
        <comment>Network adapters tooltip</comment>
        <translation type="obsolete">&lt;br&gt;&lt;nobr&gt;&lt;b&gt;Адаптер %1 (%2)&lt;/b&gt;: %3 кабель %4&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>ACPI Sh&amp;utdown</source>
        <translation type="obsolete">В&amp;ыключить через ACPI</translation>
    </message>
    <message>
        <source>&amp;View</source>
        <translation type="obsolete">&amp;Вид</translation>
    </message>
    <message>
        <source>Preview Monitor %1</source>
        <translation type="obsolete">Предпросмотр монитора %1</translation>
    </message>
    <message>
        <source>No CD/DVD devices attached to that VM</source>
        <translation type="obsolete">Нет подсоединенных приводов оптических дисков</translation>
    </message>
    <message>
        <source>No floppy devices attached to that VM</source>
        <translation type="obsolete">Нет подсоединенных приводов гибких дисков</translation>
    </message>
    <message>
        <source>No USB Devices Connected</source>
        <translation type="obsolete">Нет подсоединенных USB устройств</translation>
    </message>
    <message>
        <source>No supported devices connected to the host PC</source>
        <translation type="obsolete">Нет поддерживаемых устройств, подключенных к хосту</translation>
    </message>
</context>
<context>
    <name>VBoxEmptyFileSelector</name>
    <message>
        <source>&amp;Choose...</source>
        <translation>&amp;Выбрать...</translation>
    </message>
</context>
<context>
    <name>VBoxFilePathSelectorWidget</name>
    <message>
        <source>&lt;reset to default&gt;</source>
        <translation>&lt;по умолчанию&gt;</translation>
    </message>
    <message>
        <source>The actual default path value will be displayed after accepting the changes and opening this dialog again.</source>
        <translation>Фактическая папка по умолчанию будет показана после принятия изменений и открытия этого диалога еще раз.</translation>
    </message>
    <message>
        <source>&lt;not selected&gt;</source>
        <translation>&lt;ничего не выбрано&gt;</translation>
    </message>
    <message>
        <source>Please use the &lt;b&gt;Other...&lt;/b&gt; item from the drop-down list to select a path.</source>
        <translation>Используйте пункт &lt;b&gt;Другой...&lt;/b&gt; из выпадающего списка для выбора требуемого пути.</translation>
    </message>
    <message>
        <source>Other...</source>
        <translation>Другой...</translation>
    </message>
    <message>
        <source>Reset</source>
        <translation>Сбросить</translation>
    </message>
    <message>
        <source>Opens a dialog to select a different folder.</source>
        <translation>Открывает диалог для выбора папки.</translation>
    </message>
    <message>
        <source>Resets the folder path to the default value.</source>
        <translation>Устанавливает путь к папке, используемый по умолчанию.</translation>
    </message>
    <message>
        <source>Opens a dialog to select a different file.</source>
        <translation>Открывает диалог для выбора файла.</translation>
    </message>
    <message>
        <source>Resets the file path to the default value.</source>
        <translation>Устанавливает путь к файлу, используемый по умолчанию.</translation>
    </message>
    <message>
        <source>&amp;Copy</source>
        <translation>&amp;Копировать</translation>
    </message>
    <message>
        <source>Please type the folder path here.</source>
        <translation>Введите путь к требуемой папке.</translation>
    </message>
    <message>
        <source>Please type the file path here.</source>
        <translation>Введите путь к требуемому файлу.</translation>
    </message>
</context>
<context>
    <name>VBoxGLSettingsDlg</name>
    <message>
        <source>General</source>
        <translation type="obsolete">Общие</translation>
    </message>
    <message>
        <source>Input</source>
        <translation type="obsolete">Ввод</translation>
    </message>
    <message>
        <source>Update</source>
        <translation type="obsolete">Обновления</translation>
    </message>
    <message>
        <source>Language</source>
        <translation type="obsolete">Язык</translation>
    </message>
    <message>
        <source>USB</source>
        <translation type="obsolete">USB</translation>
    </message>
    <message>
        <source>VirtualBox - %1</source>
        <translation type="obsolete">VirtualBox - %1</translation>
    </message>
    <message>
        <source>Network</source>
        <translation type="obsolete">Сеть</translation>
    </message>
</context>
<context>
    <name>VBoxGLSettingsInput</name>
    <message>
        <source>Host &amp;Key:</source>
        <translation type="obsolete">&amp;Хост-клавиша:</translation>
    </message>
    <message>
        <source>Displays the key used as a Host Key in the VM window. Activate the entry field and press a new Host Key. Note that alphanumeric, cursor movement and editing keys cannot be used.</source>
        <translation type="obsolete">Показывает клавишу, используемую в качестве хост-клавиши в окне ВМ. Активируйте поле ввода и нажмите новую хост-клавишу. Нельзя использовать буквенные, цифровые клавиши, клавиши управления курсором и редактирования.</translation>
    </message>
    <message>
        <source>When checked, the keyboard is automatically captured every time the VM window is activated. When the keyboard is captured, all keystrokes (including system ones like Alt-Tab) are directed to the VM.</source>
        <translation type="obsolete">Когда стоит галочка, происходит автоматический захват клавиатуры при каждом переключении в окно ВМ. Когда клавиатура захвачена, все нажатия клавиш (включая системные, такие как Alt-Tab), направляются в ВМ.</translation>
    </message>
    <message>
        <source>&amp;Auto Capture Keyboard</source>
        <translation type="obsolete">А&amp;втозахват клавиатуры</translation>
    </message>
    <message>
        <source>Reset Host Key</source>
        <translation type="obsolete">Сбросить</translation>
    </message>
    <message>
        <source>Resets the key used as a Host Key in the VM window.</source>
        <translation type="obsolete">Сбрасывает назначенную хост-клавишу в значение &apos;не установлено&apos;.</translation>
    </message>
</context>
<context>
    <name>VBoxGlobal</name>
    <message>
        <source>Unknown device %1:%2</source>
        <comment>USB device details</comment>
        <translation>Неизвестное устройство %1:%2</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;Vendor ID: %1&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;Product ID: %2&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;Revision: %3&lt;/nobr&gt;</source>
        <comment>USB device tooltip</comment>
        <translation>&lt;nobr&gt;ID поставщика: %1&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;ID продукта: %2&lt;/nobr&gt;&lt;br&gt;&lt;nobr&gt;Ревизия: %3&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;br&gt;&lt;nobr&gt;Serial No. %1&lt;/nobr&gt;</source>
        <comment>USB device tooltip</comment>
        <translation>&lt;br&gt;&lt;nobr&gt;Серийный № %1&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;br&gt;&lt;nobr&gt;State: %1&lt;/nobr&gt;</source>
        <comment>USB device tooltip</comment>
        <translation>&lt;br&gt;&lt;nobr&gt;Состояние: %1&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>Name</source>
        <comment>details report</comment>
        <translation>Имя</translation>
    </message>
    <message>
        <source>OS Type</source>
        <comment>details report</comment>
        <translation>Тип ОС</translation>
    </message>
    <message>
        <source>Base Memory</source>
        <comment>details report</comment>
        <translation>Основная память</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;%3 MB&lt;/nobr&gt;</source>
        <comment>details report</comment>
        <translation type="obsolete">&lt;nobr&gt;%3 Мб&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>General</source>
        <comment>details report</comment>
        <translation>Общие</translation>
    </message>
    <message>
        <source>Video Memory</source>
        <comment>details report</comment>
        <translation>Видеопамять</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;%4 MB&lt;/nobr&gt;</source>
        <comment>details report</comment>
        <translation type="obsolete">&lt;nobr&gt;%4 Мб&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>Boot Order</source>
        <comment>details report</comment>
        <translation>Порядок загрузки</translation>
    </message>
    <message>
        <source>ACPI</source>
        <comment>details report</comment>
        <translation>ACPI</translation>
    </message>
    <message>
        <source>IO APIC</source>
        <comment>details report</comment>
        <translation>IO APIC</translation>
    </message>
    <message>
        <source>Not Attached</source>
        <comment>details report (HDDs)</comment>
        <translation type="obsolete">Не подсоединены</translation>
    </message>
    <message>
        <source>Hard Disks</source>
        <comment>details report</comment>
        <translation type="obsolete">Жесткие диски</translation>
    </message>
    <message>
        <source>Enabled</source>
        <comment>details report (ACPI)</comment>
        <translation>Включен</translation>
    </message>
    <message>
        <source>Disabled</source>
        <comment>details report (ACPI)</comment>
        <translation>Выключен</translation>
    </message>
    <message>
        <source>Enabled</source>
        <comment>details report (IO APIC)</comment>
        <translation>Включен</translation>
    </message>
    <message>
        <source>Disabled</source>
        <comment>details report (IO APIC)</comment>
        <translation>Выключен</translation>
    </message>
    <message>
        <source>Not mounted</source>
        <comment>details report (floppy)</comment>
        <translation type="obsolete">Не подключена</translation>
    </message>
    <message>
        <source>Image</source>
        <comment>details report (floppy)</comment>
        <translation type="obsolete">Образ</translation>
    </message>
    <message>
        <source>Host Drive</source>
        <comment>details report (floppy)</comment>
        <translation type="obsolete">Физический привод</translation>
    </message>
    <message>
        <source>Floppy</source>
        <comment>details report</comment>
        <translation type="obsolete">Дискета</translation>
    </message>
    <message>
        <source>Not mounted</source>
        <comment>details report (DVD)</comment>
        <translation type="obsolete">Не подключен</translation>
    </message>
    <message>
        <source>Image</source>
        <comment>details report (DVD)</comment>
        <translation type="obsolete">Образ</translation>
    </message>
    <message>
        <source>Host Drive</source>
        <comment>details report (DVD)</comment>
        <translation type="obsolete">Физический привод</translation>
    </message>
    <message>
        <source>CD/DVD-ROM</source>
        <comment>details report</comment>
        <translation type="obsolete">CD/DVD-ROM</translation>
    </message>
    <message>
        <source>Disabled</source>
        <comment>details report (audio)</comment>
        <translation>Выключено</translation>
    </message>
    <message>
        <source>Audio</source>
        <comment>details report</comment>
        <translation>Аудио</translation>
    </message>
    <message>
        <source>Adapter %1</source>
        <comment>details report (network)</comment>
        <translation>Адаптер %1</translation>
    </message>
    <message>
        <source>Disabled</source>
        <comment>details report (network)</comment>
        <translation>Выключена</translation>
    </message>
    <message>
        <source>Network</source>
        <comment>details report</comment>
        <translation>Сеть</translation>
    </message>
    <message>
        <source>Device Filters</source>
        <comment>details report (USB)</comment>
        <translation>Фильтры устройств</translation>
    </message>
    <message>
        <source>%1 (%2 active)</source>
        <comment>details report (USB)</comment>
        <translation>%1 (%2 активно)</translation>
    </message>
    <message>
        <source>Disabled</source>
        <comment>details report (USB)</comment>
        <translation>Выключен</translation>
    </message>
    <message>
        <source>VRDP Server Port</source>
        <comment>details report (VRDP)</comment>
        <translation type="obsolete">Порт VRDP-сервера</translation>
    </message>
    <message>
        <source>%1</source>
        <comment>details report (VRDP)</comment>
        <translation type="obsolete">%1</translation>
    </message>
    <message>
        <source>Disabled</source>
        <comment>details report (VRDP)</comment>
        <translation type="obsolete">Выключен</translation>
    </message>
    <message>
        <source>Remote Display</source>
        <comment>details report</comment>
        <translation type="obsolete">Удаленный дисплей</translation>
    </message>
    <message>
        <source>Opening URLs is not implemented yet.</source>
        <translation type="obsolete">Открытие URL пока еще не реализовано.</translation>
    </message>
    <message>
        <source>Powered Off</source>
        <comment>MachineState</comment>
        <translation>Выключена</translation>
    </message>
    <message>
        <source>Saved</source>
        <comment>MachineState</comment>
        <translation>Сохранена</translation>
    </message>
    <message>
        <source>Aborted</source>
        <comment>MachineState</comment>
        <translation>Прервана</translation>
    </message>
    <message>
        <source>Running</source>
        <comment>MachineState</comment>
        <translation>Работает</translation>
    </message>
    <message>
        <source>Paused</source>
        <comment>MachineState</comment>
        <translation>Приостановлена</translation>
    </message>
    <message>
        <source>Starting</source>
        <comment>MachineState</comment>
        <translation>Запускается</translation>
    </message>
    <message>
        <source>Stopping</source>
        <comment>MachineState</comment>
        <translation>Останавливается</translation>
    </message>
    <message>
        <source>Saving</source>
        <comment>MachineState</comment>
        <translation>Сохраняется</translation>
    </message>
    <message>
        <source>Restoring</source>
        <comment>MachineState</comment>
        <translation>Восстанавливается</translation>
    </message>
    <message>
        <source>Discarding</source>
        <comment>MachineState</comment>
        <translation type="obsolete">Cбрасывается</translation>
    </message>
    <message>
        <source>Closed</source>
        <comment>SessionState</comment>
        <translation type="obsolete">Закрыта</translation>
    </message>
    <message>
        <source>Open</source>
        <comment>SessionState</comment>
        <translation type="obsolete">Открыта</translation>
    </message>
    <message>
        <source>Spawning</source>
        <comment>SessionState</comment>
        <translation>Открывается</translation>
    </message>
    <message>
        <source>Closing</source>
        <comment>SessionState</comment>
        <translation type="obsolete">Закрывается</translation>
    </message>
    <message>
        <source>None</source>
        <comment>DeviceType</comment>
        <translation>Нет устройства</translation>
    </message>
    <message>
        <source>Floppy</source>
        <comment>DeviceType</comment>
        <translation>Дискета</translation>
    </message>
    <message>
        <source>CD/DVD-ROM</source>
        <comment>DeviceType</comment>
        <translation>CD/DVD-ROM</translation>
    </message>
    <message>
        <source>Hard Disk</source>
        <comment>DeviceType</comment>
        <translation>Жесткий диск</translation>
    </message>
    <message>
        <source>Network</source>
        <comment>DeviceType</comment>
        <translation>Сеть</translation>
    </message>
    <message>
        <source>Normal</source>
        <comment>DiskType</comment>
        <translation>Обычный</translation>
    </message>
    <message>
        <source>Immutable</source>
        <comment>DiskType</comment>
        <translation>Неизменяемый</translation>
    </message>
    <message>
        <source>Writethrough</source>
        <comment>DiskType</comment>
        <translation>Сквозной</translation>
    </message>
    <message>
        <source>Null</source>
        <comment>VRDPAuthType</comment>
        <translation type="obsolete">Нет авторизации</translation>
    </message>
    <message>
        <source>External</source>
        <comment>VRDPAuthType</comment>
        <translation type="obsolete">Внешняя</translation>
    </message>
    <message>
        <source>Guest</source>
        <comment>VRDPAuthType</comment>
        <translation type="obsolete">Готевая ОС</translation>
    </message>
    <message>
        <source>Ignore</source>
        <comment>USBFilterActionType</comment>
        <translation>Игнорировать</translation>
    </message>
    <message>
        <source>Hold</source>
        <comment>USBFilterActionType</comment>
        <translation>Удержать</translation>
    </message>
    <message>
        <source>Null Audio Driver</source>
        <comment>AudioDriverType</comment>
        <translation>Пустой аудиодрайвер</translation>
    </message>
    <message>
        <source>Windows Multimedia</source>
        <comment>AudioDriverType</comment>
        <translation>Windows Multimedia</translation>
    </message>
    <message>
        <source>OSS Audio Driver</source>
        <comment>AudioDriverType</comment>
        <translation>Аудиодрайвер OSS</translation>
    </message>
    <message>
        <source>ALSA Audio Driver</source>
        <comment>AudioDriverType</comment>
        <translation>Аудиодрайвер ALSA</translation>
    </message>
    <message>
        <source>Windows DirectSound</source>
        <comment>AudioDriverType</comment>
        <translation>Windows DirectSound</translation>
    </message>
    <message>
        <source>CoreAudio</source>
        <comment>AudioDriverType</comment>
        <translation>CoreAudio</translation>
    </message>
    <message>
        <source>Not attached</source>
        <comment>NetworkAttachmentType</comment>
        <translation>Не подключен</translation>
    </message>
    <message>
        <source>NAT</source>
        <comment>NetworkAttachmentType</comment>
        <translation>NAT</translation>
    </message>
    <message>
        <source>Host Interface</source>
        <comment>NetworkAttachmentType</comment>
        <translation type="obsolete">Хост-интерфейс</translation>
    </message>
    <message>
        <source>Internal Network</source>
        <comment>NetworkAttachmentType</comment>
        <translation>Внутренняя сеть</translation>
    </message>
    <message>
        <source>Not supported</source>
        <comment>USBDeviceState</comment>
        <translation>Не поддерживается</translation>
    </message>
    <message>
        <source>Unavailable</source>
        <comment>USBDeviceState</comment>
        <translation>Недоступно</translation>
    </message>
    <message>
        <source>Busy</source>
        <comment>USBDeviceState</comment>
        <translation>Занято</translation>
    </message>
    <message>
        <source>Available</source>
        <comment>USBDeviceState</comment>
        <translation>Доступно</translation>
    </message>
    <message>
        <source>Held</source>
        <comment>USBDeviceState</comment>
        <translation>Удерживается</translation>
    </message>
    <message>
        <source>Captured</source>
        <comment>USBDeviceState</comment>
        <translation>Захвачено</translation>
    </message>
    <message>
        <source>Disabled</source>
        <comment>ClipboardType</comment>
        <translation>Выключен</translation>
    </message>
    <message>
        <source>Host To Guest</source>
        <comment>ClipboardType</comment>
        <translation>Из основной в гостевую ОС</translation>
    </message>
    <message>
        <source>Guest To Host</source>
        <comment>ClipboardType</comment>
        <translation>Из гостевой в основную ОС</translation>
    </message>
    <message>
        <source>Bidirectional</source>
        <comment>ClipboardType</comment>
        <translation>Двунаправленный</translation>
    </message>
    <message>
        <source>Select a directory</source>
        <translation type="obsolete">Выберите каталог</translation>
    </message>
    <message>
        <source>Select a file</source>
        <translation type="obsolete">Выберите файл</translation>
    </message>
    <message>
        <source>Port %1</source>
        <comment>details report (serial ports)</comment>
        <translation>Порт %1</translation>
    </message>
    <message>
        <source>Disabled</source>
        <comment>details report (serial ports)</comment>
        <translation>Выключены</translation>
    </message>
    <message>
        <source>Serial Ports</source>
        <comment>details report</comment>
        <translation>COM-порты</translation>
    </message>
    <message>
        <source>USB</source>
        <comment>details report</comment>
        <translation>USB</translation>
    </message>
    <message>
        <source>Shared Folders</source>
        <comment>details report (shared folders)</comment>
        <translation>Общие папки</translation>
    </message>
    <message>
        <source>%1</source>
        <comment>details report (shadef folders)</comment>
        <translation type="obsolete">%1</translation>
    </message>
    <message>
        <source>None</source>
        <comment>details report (shared folders)</comment>
        <translation>Нет</translation>
    </message>
    <message>
        <source>Shared Folders</source>
        <comment>details report</comment>
        <translation>Общие папки</translation>
    </message>
    <message>
        <source>Stuck</source>
        <comment>MachineState</comment>
        <translation type="obsolete">Зависла</translation>
    </message>
    <message>
        <source>Disconnected</source>
        <comment>PortMode</comment>
        <translation>Отключен</translation>
    </message>
    <message>
        <source>Host Pipe</source>
        <comment>PortMode</comment>
        <translation>Хост-канал</translation>
    </message>
    <message>
        <source>Host Device</source>
        <comment>PortMode</comment>
        <translation>Хост-устройство</translation>
    </message>
    <message>
        <source>User-defined</source>
        <comment>serial port</comment>
        <translation>Пользовательский</translation>
    </message>
    <message>
        <source>VT-x/AMD-V</source>
        <comment>details report</comment>
        <translation>VT-x/AMD-V</translation>
    </message>
    <message>
        <source>PAE/NX</source>
        <comment>details report</comment>
        <translation>PAE/NX</translation>
    </message>
    <message>
        <source>Enabled</source>
        <comment>details report (VT-x/AMD-V)</comment>
        <translation>Включены</translation>
    </message>
    <message>
        <source>Disabled</source>
        <comment>details report (VT-x/AMD-V)</comment>
        <translation>Выключены</translation>
    </message>
    <message>
        <source>Enabled</source>
        <comment>details report (PAE/NX)</comment>
        <translation>Включена</translation>
    </message>
    <message>
        <source>Disabled</source>
        <comment>details report (PAE/NX)</comment>
        <translation>Выключена</translation>
    </message>
    <message>
        <source>Host Driver</source>
        <comment>details report (audio)</comment>
        <translation>Аудиодрайвер</translation>
    </message>
    <message>
        <source>Controller</source>
        <comment>details report (audio)</comment>
        <translation>Контроллер</translation>
    </message>
    <message>
        <source>Port %1</source>
        <comment>details report (parallel ports)</comment>
        <translation>Порт %1</translation>
    </message>
    <message>
        <source>Disabled</source>
        <comment>details report (parallel ports)</comment>
        <translation>Выключены</translation>
    </message>
    <message>
        <source>Parallel Ports</source>
        <comment>details report</comment>
        <translation>LPT-порты</translation>
    </message>
    <message>
        <source>USB</source>
        <comment>DeviceType</comment>
        <translation>USB</translation>
    </message>
    <message>
        <source>Shared Folder</source>
        <comment>DeviceType</comment>
        <translation>Обшая папка</translation>
    </message>
    <message>
        <source>IDE</source>
        <comment>StorageBus</comment>
        <translation>IDE</translation>
    </message>
    <message>
        <source>SATA</source>
        <comment>StorageBus</comment>
        <translation>SATA</translation>
    </message>
    <message>
        <source>Primary</source>
        <comment>StorageBusChannel</comment>
        <translation>первичный</translation>
    </message>
    <message>
        <source>Secondary</source>
        <comment>StorageBusChannel</comment>
        <translation>вторичный</translation>
    </message>
    <message>
        <source>Master</source>
        <comment>StorageBusDevice</comment>
        <translation>мастер</translation>
    </message>
    <message>
        <source>Slave</source>
        <comment>StorageBusDevice</comment>
        <translation>слейв</translation>
    </message>
    <message>
        <source>Port %1</source>
        <comment>StorageBusChannel</comment>
        <translation>порт %1</translation>
    </message>
    <message>
        <source>Solaris Audio</source>
        <comment>AudioDriverType</comment>
        <translation>Solaris Audio</translation>
    </message>
    <message>
        <source>PulseAudio</source>
        <comment>AudioDriverType</comment>
        <translation>PulseAudio</translation>
    </message>
    <message>
        <source>ICH AC97</source>
        <comment>AudioControllerType</comment>
        <translation>ICH AC97</translation>
    </message>
    <message>
        <source>SoundBlaster 16</source>
        <comment>AudioControllerType</comment>
        <translation>SoundBlaster 16</translation>
    </message>
    <message>
        <source>PCnet-PCI II (Am79C970A)</source>
        <comment>NetworkAdapterType</comment>
        <translation>PCnet-PCI II (Am79C970A)</translation>
    </message>
    <message>
        <source>PCnet-FAST III (Am79C973)</source>
        <comment>NetworkAdapterType</comment>
        <translation>PCnet-FAST III (Am79C973)</translation>
    </message>
    <message>
        <source>Intel PRO/1000 MT Desktop (82540EM)</source>
        <comment>NetworkAdapterType</comment>
        <translation>Intel PRO/1000 MT Desktop (82540EM)</translation>
    </message>
    <message>
        <source>Intel PRO/1000 T Server (82543GC)</source>
        <comment>NetworkAdapterType</comment>
        <translation>Intel PRO/1000 T Server (82543GC)</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;Vendor ID: %1&lt;/nobr&gt;</source>
        <comment>USB filter tooltip</comment>
        <translation>&lt;nobr&gt;ID поставщика:  %1&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;Product ID: %2&lt;/nobr&gt;</source>
        <comment>USB filter tooltip</comment>
        <translation>&lt;nobr&gt;ID продукта: %2&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;Revision: %3&lt;/nobr&gt;</source>
        <comment>USB filter tooltip</comment>
        <translation>&lt;nobr&gt;Ревизия: %3&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;Product: %4&lt;/nobr&gt;</source>
        <comment>USB filter tooltip</comment>
        <translation>&lt;nobr&gt;Продукт: %4&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;Manufacturer: %5&lt;/nobr&gt;</source>
        <comment>USB filter tooltip</comment>
        <translation>&lt;nobr&gt;Производитель: %5&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;Serial No.: %1&lt;/nobr&gt;</source>
        <comment>USB filter tooltip</comment>
        <translation>&lt;nobr&gt;Серийный №: %1&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;Port: %1&lt;/nobr&gt;</source>
        <comment>USB filter tooltip</comment>
        <translation>&lt;nobr&gt;Порт: %1&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;State: %1&lt;/nobr&gt;</source>
        <comment>USB filter tooltip</comment>
        <translation>&lt;nobr&gt;Состояние: %1&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>host interface, %1</source>
        <comment>details report (network)</comment>
        <translation type="obsolete">хост-интерфейс, %1</translation>
    </message>
    <message>
        <source>internal network, &apos;%1&apos;</source>
        <comment>details report (network)</comment>
        <translation type="obsolete">внутренняя сеть, &apos;%1&apos;</translation>
    </message>
    <message>
        <source>Adapter %1</source>
        <comment>network</comment>
        <translation type="obsolete">Адаптер %1</translation>
    </message>
    <message>
        <source>&lt;br&gt;&lt;nobr&gt;Type&amp;nbsp;(Format):&amp;nbsp;&amp;nbsp;%2&amp;nbsp;(%3)&lt;/nobr&gt;</source>
        <comment>hard disk</comment>
        <translation type="obsolete">&lt;br&gt;&lt;nobr&gt;Тип&amp;nbsp;(Формат):&amp;nbsp;&amp;nbsp;%2&amp;nbsp;(%3)&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;br&gt;&lt;nobr&gt;Attached to:&amp;nbsp;&amp;nbsp;%1&lt;/nobr&gt;</source>
        <comment>medium</comment>
        <translation type="obsolete">&lt;br&gt;&lt;nobr&gt;Подсоединен к:&amp;nbsp;&amp;nbsp;%1&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;i&gt;Not&amp;nbsp;Attached&lt;/i&gt;</source>
        <comment>medium</comment>
        <translation type="obsolete">&lt;i&gt;Не&amp;nbsp;подсоединен&lt;/i&gt;</translation>
    </message>
    <message>
        <source>&lt;br&gt;&lt;i&gt;Checking accessibility...&lt;/i&gt;</source>
        <comment>medium</comment>
        <translation type="obsolete">&lt;i&gt;Проверка доступности...&lt;/i&gt;</translation>
    </message>
    <message>
        <source>&lt;hr&gt;Failed to check media accessibility.&lt;br&gt;%1.</source>
        <comment>medium</comment>
        <translation type="obsolete">&lt;hr&gt;Не удалось провести проверку доступности носителя.&lt;br&gt;%1.</translation>
    </message>
    <message>
        <source>&lt;hr&gt;&lt;img src=%1/&gt;&amp;nbsp;Attaching this hard disk will be performed indirectly using a newly created differencing hard disk.</source>
        <comment>medium</comment>
        <translation type="obsolete">&lt;hr&gt;&lt;img src=%1/&gt;&amp;nbsp;Данный жесткий диск будет подсоединен косвенно с помощью создания нового разностного жесткого диска.</translation>
    </message>
    <message>
        <source>Checking...</source>
        <comment>medium</comment>
        <translation>Проверка...</translation>
    </message>
    <message>
        <source>Inaccessible</source>
        <comment>medium</comment>
        <translation>Недоступен</translation>
    </message>
    <message>
        <source>&lt;hr&gt;Some of the media in this hard disk chain are inaccessible. Please use the Virtual Media Manager in &lt;b&gt;Show Differencing Hard Disks&lt;/b&gt; mode to inspect these media.</source>
        <translation type="obsolete">&lt;hr&gt;Некоторые жесткие диски в данной цепочке недоступны. Используйте Менеджер виртуальных носителей в режиме &lt;b&gt;Показывать разностные жесткие диски&lt;/b&gt; для просмотра подробной информации об этих жестких дисках.</translation>
    </message>
    <message>
        <source>%1&lt;hr&gt;This base hard disk is indirectly attached using the following differencing hard disk:&lt;br&gt;%2%3</source>
        <translation type="obsolete">%1&lt;hr&gt;Этот базовый жесткий диск подключен косвенно с помощью следующего разностного жесткого диска:&lt;br&gt;%2%3</translation>
    </message>
    <message>
        <source>3D Acceleration</source>
        <comment>details report</comment>
        <translation>3D-ускорение</translation>
    </message>
    <message>
        <source>Enabled</source>
        <comment>details report (3D Acceleration)</comment>
        <translation>Включено</translation>
    </message>
    <message>
        <source>Disabled</source>
        <comment>details report (3D Acceleration)</comment>
        <translation>Выключено</translation>
    </message>
    <message>
        <source>Setting Up</source>
        <comment>MachineState</comment>
        <translation>Настройка</translation>
    </message>
    <message>
        <source>Differencing</source>
        <comment>DiskType</comment>
        <translation>Разностный</translation>
    </message>
    <message>
        <source>Nested Paging</source>
        <comment>details report</comment>
        <translation>Nested Paging</translation>
    </message>
    <message>
        <source>Enabled</source>
        <comment>details report (Nested Paging)</comment>
        <translation>Включено</translation>
    </message>
    <message>
        <source>Disabled</source>
        <comment>details report (Nested Paging)</comment>
        <translation>Выключено</translation>
    </message>
    <message>
        <source>Bridged network, %1</source>
        <comment>details report (network)</comment>
        <translation type="obsolete">Сетевой мост, &apos;%1&apos;</translation>
    </message>
    <message>
        <source>Internal network, &apos;%1&apos;</source>
        <comment>details report (network)</comment>
        <translation>Внутренняя сеть, &apos;%1&apos;</translation>
    </message>
    <message>
        <source>Host-only network, &apos;%1&apos;</source>
        <comment>details report (network)</comment>
        <translation type="obsolete">Виртуальная сеть хоста, &apos;%1&apos;</translation>
    </message>
    <message>
        <source>SCSI</source>
        <comment>StorageBus</comment>
        <translation>SCSI</translation>
    </message>
    <message>
        <source>Bridged Network</source>
        <comment>NetworkAttachmentType</comment>
        <translation type="obsolete">Сетевой мост</translation>
    </message>
    <message>
        <source>Host-only Network</source>
        <comment>NetworkAttachmentType</comment>
        <translation type="obsolete">Виртуальная сеть хоста</translation>
    </message>
    <message>
        <source>PIIX3</source>
        <comment>StorageControllerType</comment>
        <translation>PIIX3</translation>
    </message>
    <message>
        <source>PIIX4</source>
        <comment>StorageControllerType</comment>
        <translation>PIIX4</translation>
    </message>
    <message>
        <source>ICH6</source>
        <comment>StorageControllerType</comment>
        <translation>ICH6</translation>
    </message>
    <message>
        <source>AHCI</source>
        <comment>StorageControllerType</comment>
        <translation>AHCI</translation>
    </message>
    <message>
        <source>Lsilogic</source>
        <comment>StorageControllerType</comment>
        <translation>Lsilogic</translation>
    </message>
    <message>
        <source>BusLogic</source>
        <comment>StorageControllerType</comment>
        <translation>BusLogic</translation>
    </message>
    <message>
        <source>Intel PRO/1000 MT Server (82545EM)</source>
        <comment>NetworkAdapterType</comment>
        <translation>Intel PRO/1000 MT Server (82545EM)</translation>
    </message>
    <message>
        <source>Bridged adapter, %1</source>
        <comment>details report (network)</comment>
        <translation>Сетевой мост, &apos;%1&apos;</translation>
    </message>
    <message>
        <source>Host-only adapter, &apos;%1&apos;</source>
        <comment>details report (network)</comment>
        <translation>Виртуальный адаптер хоста, &apos;%1&apos;</translation>
    </message>
    <message>
        <source>Bridged Adapter</source>
        <comment>NetworkAttachmentType</comment>
        <translation>Сетевой мост</translation>
    </message>
    <message>
        <source>Host-only Adapter</source>
        <comment>NetworkAttachmentType</comment>
        <translation>Виртуальный адаптер хоста</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;%1 MB&lt;/nobr&gt;</source>
        <comment>details report</comment>
        <translation>&lt;nobr&gt;%1 МБ&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>Processor(s)</source>
        <comment>details report</comment>
        <translation>Процессор(ы)</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;%1&lt;/nobr&gt;</source>
        <comment>details report</comment>
        <translation>&lt;nobr&gt;%1&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>System</source>
        <comment>details report</comment>
        <translation>Система</translation>
    </message>
    <message>
        <source>Remote Display Server Port</source>
        <comment>details report (VRDP Server)</comment>
        <translation type="obsolete">Порт сервера удалённого дисплея</translation>
    </message>
    <message>
        <source>Remote Display Server</source>
        <comment>details report (VRDP Server)</comment>
        <translation type="obsolete">Сервер удалённого дисплея</translation>
    </message>
    <message>
        <source>Disabled</source>
        <comment>details report (VRDP Server)</comment>
        <translation type="obsolete">Выключен</translation>
    </message>
    <message>
        <source>Display</source>
        <comment>details report</comment>
        <translation>Дисплей</translation>
    </message>
    <message>
        <source>Raw File</source>
        <comment>PortMode</comment>
        <translation>Перенаправлен в файл</translation>
    </message>
    <message>
        <source>Enabled</source>
        <comment>details report (2D Video Acceleration)</comment>
        <translation>Включено</translation>
    </message>
    <message>
        <source>Disabled</source>
        <comment>details report (2D Video Acceleration)</comment>
        <translation>Выключено</translation>
    </message>
    <message>
        <source>2D Video Acceleration</source>
        <comment>details report</comment>
        <translation>2D-ускорение видео</translation>
    </message>
    <message>
        <source>Not Attached</source>
        <comment>details report (Storage)</comment>
        <translation>Не подсоединены</translation>
    </message>
    <message>
        <source>Storage</source>
        <comment>details report</comment>
        <translation>Носители</translation>
    </message>
    <message>
        <source>Teleported</source>
        <comment>MachineState</comment>
        <translation>Портирована</translation>
    </message>
    <message>
        <source>Guru Meditation</source>
        <comment>MachineState</comment>
        <translation>Критическая ошибка</translation>
    </message>
    <message>
        <source>Teleporting</source>
        <comment>MachineState</comment>
        <translation>Портируется</translation>
    </message>
    <message>
        <source>Taking Live Snapshot</source>
        <comment>MachineState</comment>
        <translation>Создание рабочего снимка</translation>
    </message>
    <message>
        <source>Teleporting Paused VM</source>
        <comment>MachineState</comment>
        <translation>Портируется приостановленная машина</translation>
    </message>
    <message>
        <source>Restoring Snapshot</source>
        <comment>MachineState</comment>
        <translation>Восстанавливается снимок</translation>
    </message>
    <message>
        <source>Deleting Snapshot</source>
        <comment>MachineState</comment>
        <translation>Удаляется снимок</translation>
    </message>
    <message>
        <source>Floppy</source>
        <comment>StorageBus</comment>
        <translation>Floppy</translation>
    </message>
    <message>
        <source>Device %1</source>
        <comment>StorageBusDevice</comment>
        <translation>Устройство %1</translation>
    </message>
    <message>
        <source>IDE Primary Master</source>
        <comment>New Storage UI : Slot Name</comment>
        <translation>Первичный мастер IDE</translation>
    </message>
    <message>
        <source>IDE Primary Slave</source>
        <comment>New Storage UI : Slot Name</comment>
        <translation>Первичный слэйв IDE</translation>
    </message>
    <message>
        <source>IDE Secondary Master</source>
        <comment>New Storage UI : Slot Name</comment>
        <translation>Вторичный мастер IDE</translation>
    </message>
    <message>
        <source>IDE Secondary Slave</source>
        <comment>New Storage UI : Slot Name</comment>
        <translation>Вторичный слэйв IDE</translation>
    </message>
    <message>
        <source>SATA Port %1</source>
        <comment>New Storage UI : Slot Name</comment>
        <translation>SATA порт %1</translation>
    </message>
    <message>
        <source>SCSI Port %1</source>
        <comment>New Storage UI : Slot Name</comment>
        <translation>SCSI порт %1</translation>
    </message>
    <message>
        <source>Floppy Device %1</source>
        <comment>New Storage UI : Slot Name</comment>
        <translation>Floppy привод %1</translation>
    </message>
    <message>
        <source>Paravirtualized Network (virtio-net)</source>
        <comment>NetworkAdapterType</comment>
        <translation>Паравиртуальная сеть (virtio-net)</translation>
    </message>
    <message>
        <source>I82078</source>
        <comment>StorageControllerType</comment>
        <translation>I82078</translation>
    </message>
    <message>
        <source>Empty</source>
        <comment>medium</comment>
        <translation>Пусто</translation>
    </message>
    <message>
        <source>Host Drive &apos;%1&apos;</source>
        <comment>medium</comment>
        <translation>Привод хоста &apos;%1&apos;</translation>
    </message>
    <message>
        <source>Host Drive %1 (%2)</source>
        <comment>medium</comment>
        <translation>Привод хоста &apos;%1&apos; (%2)</translation>
    </message>
    <message>
        <source>&lt;p style=white-space:pre&gt;Type (Format):  %1 (%2)&lt;/p&gt;</source>
        <comment>medium</comment>
        <translation>&lt;p style=white-space:pre&gt;Тип (Формат):  %1 (%2)&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Attached to:  %1&lt;/p&gt;</source>
        <comment>image</comment>
        <translation>&lt;p&gt;Подсоединён к:  %1&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;i&gt;Not Attached&lt;/i&gt;</source>
        <comment>image</comment>
        <translation>&lt;i&gt;Не подсоединён&lt;/i&gt;</translation>
    </message>
    <message>
        <source>&lt;i&gt;Checking accessibility...&lt;/i&gt;</source>
        <comment>medium</comment>
        <translation>&lt;i&gt;Проверка доступности...&lt;/i&gt;</translation>
    </message>
    <message>
        <source>Failed to check media accessibility.</source>
        <comment>medium</comment>
        <translation>Не удалось проверить доступность устройства.</translation>
    </message>
    <message>
        <source>&lt;b&gt;No medium selected&lt;/b&gt;</source>
        <comment>medium</comment>
        <translation>&lt;b&gt;Устройство не выбрано&lt;/b&gt;</translation>
    </message>
    <message>
        <source>You can also change this while the machine is running.</source>
        <translation>Вы можете выбрать это устройство позже или во время работы машины.</translation>
    </message>
    <message>
        <source>&lt;b&gt;No media available&lt;/b&gt;</source>
        <comment>medium</comment>
        <translation>&lt;b&gt;Нет доступных устройств&lt;/b&gt;</translation>
    </message>
    <message>
        <source>You can create media images using the virtual media manager.</source>
        <translation>Вы можете добавить необходимое устойство, используя менеджер виртуальных носителей.</translation>
    </message>
    <message>
        <source>Attaching this hard disk will be performed indirectly using a newly created differencing hard disk.</source>
        <comment>medium</comment>
        <translation>Подключение данного жёсткого диска будет осуществлено косвенно, используя новый разностный диск.</translation>
    </message>
    <message>
        <source>Some of the media in this hard disk chain are inaccessible. Please use the Virtual Media Manager in &lt;b&gt;Show Differencing Hard Disks&lt;/b&gt; mode to inspect these media.</source>
        <comment>medium</comment>
        <translation>Некоторые устройства данной цепочки жёстких дисков недоступны. Для проверки вы можете использовать менеджер виртуальных носителей в режиме &lt;b&gt;отображения разностных жёстких дисков&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>This base hard disk is indirectly attached using the following differencing hard disk:</source>
        <comment>medium</comment>
        <translation>Этот базовый жёсткий диск косвенно подсоединен с помощью следующего разностного диска:</translation>
    </message>
    <message numerus="yes">
        <source>%n year(s)</source>
        <translation>
            <numerusform>%n год</numerusform>
            <numerusform>%n года</numerusform>
            <numerusform>%n лет</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>%n month(s)</source>
        <translation>
            <numerusform>%n месяц</numerusform>
            <numerusform>%n месяца</numerusform>
            <numerusform>%n месяцев</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>%n day(s)</source>
        <translation>
            <numerusform>%n день</numerusform>
            <numerusform>%n дня</numerusform>
            <numerusform>%n дней</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>%n hour(s)</source>
        <translation>
            <numerusform>%n час</numerusform>
            <numerusform>%n часа</numerusform>
            <numerusform>%n часов</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>%n minute(s)</source>
        <translation>
            <numerusform>%n минута</numerusform>
            <numerusform>%n минуты</numerusform>
            <numerusform>%n минут</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>%n second(s)</source>
        <translation>
            <numerusform>%n секунда</numerusform>
            <numerusform>%n секунды</numerusform>
            <numerusform>%n секунд</numerusform>
        </translation>
    </message>
    <message>
        <source>(CD/DVD)</source>
        <translation>(CD/DVD)</translation>
    </message>
    <message>
        <source>Screens</source>
        <comment>details report</comment>
        <translation>Мониторы</translation>
    </message>
    <message>
        <source>VDE network, &apos;%1&apos;</source>
        <comment>details report (network)</comment>
        <translation type="obsolete">VDE-сеть, &apos;%1&apos;</translation>
    </message>
    <message>
        <source>SAS</source>
        <comment>StorageBus</comment>
        <translation>SAS</translation>
    </message>
    <message>
        <source>VDE Adapter</source>
        <comment>NetworkAttachmentType</comment>
        <translation type="obsolete">VDE-Адаптер</translation>
    </message>
    <message>
        <source>LsiLogic SAS</source>
        <comment>StorageControllerType</comment>
        <translation>LsiLogic SAS</translation>
    </message>
    <message>
        <source>^(?:(?:(\d+)(?:\s?(B|KB|MB|GB|TB|PB))?)|(?:(\d*)%1(\d{1,2})(?:\s?(KB|MB|GB|TB|PB))))$</source>
        <comment>regexp for matching ####[.##] B|KB|MB|GB|TB|PB, %1=decimal point</comment>
        <translation type="obsolete">^(?:(?:(\d+)(?:\s?(Б|КБ|МБ|ГБ|ТБ|ПБ))?)|(?:(\d*)%1(\d{1,2})(?:\s?(КБ|МБ|ГБ|ТБ|ПБ))))$</translation>
    </message>
    <message>
        <source>B</source>
        <comment>size suffix Bytes</comment>
        <translation>Б</translation>
    </message>
    <message>
        <source>KB</source>
        <comment>size suffix KBytes=1024 Bytes</comment>
        <translation>КБ</translation>
    </message>
    <message>
        <source>MB</source>
        <comment>size suffix MBytes=1024 KBytes</comment>
        <translation>МБ</translation>
    </message>
    <message>
        <source>GB</source>
        <comment>size suffix GBytes=1024 MBytes</comment>
        <translation>ГБ</translation>
    </message>
    <message>
        <source>TB</source>
        <comment>size suffix TBytes=1024 GBytes</comment>
        <translation>ТБ</translation>
    </message>
    <message>
        <source>PB</source>
        <comment>size suffix PBytes=1024 TBytes</comment>
        <translation>ПБ</translation>
    </message>
    <message>
        <source>Enabled</source>
        <comment>nested paging</comment>
        <translation>Включено</translation>
    </message>
    <message>
        <source>Disabled</source>
        <comment>nested paging</comment>
        <translation>Выключено</translation>
    </message>
    <message>
        <source>Nested Paging</source>
        <translation>Nested Paging</translation>
    </message>
    <message>
        <source>Shareable</source>
        <comment>DiskType</comment>
        <translation>С общим доступом</translation>
    </message>
    <message>
        <source>Unknown device</source>
        <comment>USB device details</comment>
        <translation>Неизвестное устройство</translation>
    </message>
    <message>
        <source>SAS Port %1</source>
        <comment>New Storage UI : Slot Name</comment>
        <translation>SAS порт %1</translation>
    </message>
    <message>
        <source>Remote Desktop Server Port</source>
        <comment>details report (VRDE Server)</comment>
        <translation>Порт сервера удалённого дисплея</translation>
    </message>
    <message>
        <source>Remote Desktop Server</source>
        <comment>details report (VRDE Server)</comment>
        <translation>Сервер удалённого дисплея</translation>
    </message>
    <message>
        <source>Disabled</source>
        <comment>details report (VRDE Server)</comment>
        <translation>Выключен</translation>
    </message>
    <message>
        <source>Choose a virtual hard disk file</source>
        <translation>Выберите образ жёсткого диска</translation>
    </message>
    <message>
        <source>hard disk</source>
        <translation>жёстких дисков</translation>
    </message>
    <message>
        <source>Choose a virtual CD/DVD disk file</source>
        <translation>Выберите образ оптического диска</translation>
    </message>
    <message>
        <source>CD/DVD-ROM disk</source>
        <translation>оптических дисков</translation>
    </message>
    <message>
        <source>Choose a virtual floppy disk file</source>
        <translation>Выберите образ гибкого диска</translation>
    </message>
    <message>
        <source>floppy disk</source>
        <translation>гибких дисков</translation>
    </message>
    <message>
        <source>All %1 images (%2)</source>
        <translation>Все образы %1 (%2)</translation>
    </message>
    <message>
        <source>All files (*)</source>
        <translation>Все файлы (*)</translation>
    </message>
    <message>
        <source>Fault Tolerant Syncing</source>
        <comment>MachineState</comment>
        <translation>Синхронизация сбоя</translation>
    </message>
    <message>
        <source>Unlocked</source>
        <comment>SessionState</comment>
        <translation>Разблокировано</translation>
    </message>
    <message>
        <source>Locked</source>
        <comment>SessionState</comment>
        <translation>Заблокировано</translation>
    </message>
    <message>
        <source>Unlocking</source>
        <comment>SessionState</comment>
        <translation>Разблокируется</translation>
    </message>
    <message>
        <source>Null</source>
        <comment>AuthType</comment>
        <translation>Нет авторизации</translation>
    </message>
    <message>
        <source>External</source>
        <comment>AuthType</comment>
        <translation>Внешняя</translation>
    </message>
    <message>
        <source>Guest</source>
        <comment>AuthType</comment>
        <translation>Готевая ОС</translation>
    </message>
    <message>
        <source>Intel HD Audio</source>
        <comment>AudioControllerType</comment>
        <translation>Intel HD Audio</translation>
    </message>
    <message>
        <source>UDP</source>
        <comment>NATProtocolType</comment>
        <translation>UDP</translation>
    </message>
    <message>
        <source>TCP</source>
        <comment>NATProtocolType</comment>
        <translation>TCP</translation>
    </message>
    <message>
        <source>PIIX3</source>
        <comment>ChipsetType</comment>
        <translation>PIIX3</translation>
    </message>
    <message>
        <source>ICH9</source>
        <comment>ChipsetType</comment>
        <translation>ICH9</translation>
    </message>
    <message>
        <source>and</source>
        <translation>и</translation>
    </message>
    <message>
        <source>MB</source>
        <comment>size suffix MBytes=1024KBytes</comment>
        <translation>МБ</translation>
    </message>
    <message>
        <source>Readonly</source>
        <comment>DiskType</comment>
        <translation>Только для чтения</translation>
    </message>
    <message>
        <source>Multi-attach</source>
        <comment>DiskType</comment>
        <translation>Множественное подключение</translation>
    </message>
    <message>
        <source>Dynamically allocated storage</source>
        <translation>Динамически расширяющийся образ</translation>
    </message>
    <message>
        <source>Fixed size storage</source>
        <translation>Образ фиксированного размера</translation>
    </message>
    <message>
        <source>Dynamically allocated storage split into files of less than 2GB</source>
        <translation>Динамически расширяющийся образ, разделённый на файлы менее 2х ГБ</translation>
    </message>
    <message>
        <source>Fixed size storage split into files of less than 2GB</source>
        <translation>Образ фиксированного размера, разделённый на файлы менее 2х ГБ</translation>
    </message>
    <message>
        <source>Execution Cap</source>
        <comment>details report</comment>
        <translation>Предел загрузки ЦПУ</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;%1%&lt;/nobr&gt;</source>
        <comment>details report</comment>
        <translation>&lt;nobr&gt;%1%&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>Generic, &apos;%1&apos;</source>
        <comment>details report (network)</comment>
        <translation>Универсальный драйвер, &apos;%1&apos;</translation>
    </message>
    <message>
        <source>Generic Driver</source>
        <comment>NetworkAttachmentType</comment>
        <translation>Универсальный драйвер</translation>
    </message>
    <message>
        <source>Deny</source>
        <comment>NetworkAdapterPromiscModePolicyType</comment>
        <translation>Запретить</translation>
    </message>
    <message>
        <source>Allow VMs</source>
        <comment>NetworkAdapterPromiscModePolicyType</comment>
        <translation>Разрешить ВМ</translation>
    </message>
    <message>
        <source>Allow All</source>
        <comment>NetworkAdapterPromiscModePolicyType</comment>
        <translation>Разрешить всё</translation>
    </message>
    <message>
        <source>Adapter %1</source>
        <translation>Адаптер %1</translation>
    </message>
</context>
<context>
    <name>VBoxGlobalSettings</name>
    <message>
        <source>&apos;%1 (0x%2)&apos; is an invalid host key code.</source>
        <translation type="obsolete">Hеправильный код хост-клавиши: &apos;%1 (0x%2)&apos;.</translation>
    </message>
    <message>
        <source>The value &apos;%1&apos; of the key &apos;%2&apos; doesn&apos;t match the regexp constraint &apos;%3&apos;.</source>
        <translation>Значение &apos;%1&apos; параметра &apos;%2&apos; не совпадает с regexp-выражением &apos;%3&apos;.</translation>
    </message>
    <message>
        <source>Cannot delete the key &apos;%1&apos;.</source>
        <translation>Не удается удалить параметр &apos;%1&apos;.</translation>
    </message>
    <message>
        <source>&apos;%1&apos; is an invalid host-combination code-sequence.</source>
        <translation>&apos;%1&apos; - неверная хост-комбинация.</translation>
    </message>
</context>
<context>
    <name>VBoxHelpButton</name>
    <message>
        <source>&amp;Help</source>
        <translation type="obsolete">Справк&amp;а</translation>
    </message>
</context>
<context>
    <name>VBoxImportApplianceWgt</name>
    <message>
        <source>Importing Appliance ...</source>
        <translation>Импорт конфигурации ...</translation>
    </message>
    <message>
        <source>Reading Appliance ...</source>
        <translation>Чтение конфигурации ...</translation>
    </message>
</context>
<context>
    <name>VBoxLicenseViewer</name>
    <message>
        <source>I &amp;Agree</source>
        <translation>Я &amp;согласен</translation>
    </message>
    <message>
        <source>I &amp;Disagree</source>
        <translation>Я &amp;не согласен</translation>
    </message>
    <message>
        <source>VirtualBox License</source>
        <translation>Лицензия VirtualBox</translation>
    </message>
</context>
<context>
    <name>VBoxLineTextEdit</name>
    <message>
        <source>&amp;Edit</source>
        <translation>&amp;Изменить</translation>
    </message>
</context>
<context>
    <name>VBoxLogSearchPanel</name>
    <message>
        <source>Close the search panel</source>
        <translation>Закрыть панель поиска</translation>
    </message>
    <message>
        <source>Find </source>
        <translation>Найти</translation>
    </message>
    <message>
        <source>Enter a search string here</source>
        <translation>Введите здесь строку для поиска</translation>
    </message>
    <message>
        <source>&amp;Previous</source>
        <translation>&amp;Предыдущая</translation>
    </message>
    <message>
        <source>Search for the previous occurrence of the string</source>
        <translation>Искать предыдущий экземпляр строки</translation>
    </message>
    <message>
        <source>&amp;Next</source>
        <translation>С&amp;ледующая</translation>
    </message>
    <message>
        <source>Search for the next occurrence of the string</source>
        <translation>Искать следующий экземпляр строки</translation>
    </message>
    <message>
        <source>C&amp;ase Sensitive</source>
        <translation>С &amp;учетом регистра</translation>
    </message>
    <message>
        <source>Perform case sensitive search (when checked)</source>
        <translation>Учитывать регистр символов при поиске (когда стоит галочка)</translation>
    </message>
    <message>
        <source>String not found</source>
        <translation>Строка не найдена</translation>
    </message>
</context>
<context>
    <name>VBoxMediaComboBox</name>
    <message>
        <source>No media available. Use the Virtual Media Manager to add media of the corresponding type.</source>
        <translation type="obsolete">Отсуствуют доступные носители. Используйте Менеджер виртуальных носителей для добавления носителей соответствующего типа.</translation>
    </message>
    <message>
        <source>&lt;no media&gt;</source>
        <translation type="obsolete">&lt;нет носителей&gt;</translation>
    </message>
</context>
<context>
    <name>VBoxMediaManagerDlg</name>
    <message>
        <source>&amp;Actions</source>
        <translation>&amp;Действия</translation>
    </message>
    <message>
        <source>&amp;New...</source>
        <translation>&amp;Создать...</translation>
    </message>
    <message>
        <source>&amp;Add...</source>
        <translation>&amp;Добавить...</translation>
    </message>
    <message>
        <source>R&amp;emove</source>
        <translation>&amp;Удалить</translation>
    </message>
    <message>
        <source>Re&amp;lease</source>
        <translation>Ос&amp;вободить</translation>
    </message>
    <message>
        <source>Re&amp;fresh</source>
        <translation>Об&amp;новить</translation>
    </message>
    <message>
        <source>Create a new virtual hard disk</source>
        <translation>Создать новый виртуальный жесткий диск</translation>
    </message>
    <message>
        <source>Add an existing medium</source>
        <translation>Добавить существующий носитель</translation>
    </message>
    <message>
        <source>Remove the selected medium</source>
        <translation>Удалить выбранный носитель</translation>
    </message>
    <message>
        <source>Release the selected medium by detaching it from the machines</source>
        <translation>Освободить выбранный носитель, отсоединив его от машин</translation>
    </message>
    <message>
        <source>Refresh the media list</source>
        <translation>Обновить список носителей</translation>
    </message>
    <message>
        <source>Location</source>
        <translation type="obsolete">Расположение</translation>
    </message>
    <message>
        <source>Type (Format)</source>
        <translation type="obsolete">Тип (Формат)</translation>
    </message>
    <message>
        <source>Attached to</source>
        <translation type="obsolete">Подсоединен к</translation>
    </message>
    <message>
        <source>Checking accessibility</source>
        <translation>Проверка доступности</translation>
    </message>
    <message>
        <source>&amp;Select</source>
        <translation>&amp;Выбрать</translation>
    </message>
    <message>
        <source>All hard disk images (%1)</source>
        <translation type="obsolete">Все образы жестких дисков (%1)</translation>
    </message>
    <message>
        <source>All files (*)</source>
        <translation>Все файлы (*)</translation>
    </message>
    <message>
        <source>Select a hard disk image file</source>
        <translation>Выберите файл образа жесткого диска</translation>
    </message>
    <message>
        <source>CD/DVD-ROM images (*.iso);;All files (*)</source>
        <translation type="obsolete">Образы CD/DVD-ROM (*.iso);;Все файлы (*)</translation>
    </message>
    <message>
        <source>Select a CD/DVD-ROM disk image file</source>
        <translation>Выберите файл образа диска CD/DVD-ROM</translation>
    </message>
    <message>
        <source>Floppy images (*.img);;All files (*)</source>
        <translation type="obsolete">Образы дискет (*.img);;Все файлы (*)</translation>
    </message>
    <message>
        <source>Select a floppy disk image file</source>
        <translation>Выберите файл образа дискеты</translation>
    </message>
    <message>
        <source>&lt;i&gt;Not&amp;nbsp;Attached&lt;/i&gt;</source>
        <translation>&lt;i&gt;Не&amp;nbsp;подсоединен&lt;/i&gt;</translation>
    </message>
    <message>
        <source>--</source>
        <comment>no info</comment>
        <translation>--</translation>
    </message>
    <message>
        <source>Virtual Media Manager</source>
        <translation>Менеджер виртуальных носителей</translation>
    </message>
    <message>
        <source>Hard &amp;Disks</source>
        <translation>&amp;Жёсткие диски</translation>
    </message>
    <message>
        <source>Name</source>
        <translation>Имя</translation>
    </message>
    <message>
        <source>Virtual Size</source>
        <translation>Вирт. размер</translation>
    </message>
    <message>
        <source>Actual Size</source>
        <translation>Факт. размер</translation>
    </message>
    <message>
        <source>&amp;CD/DVD Images</source>
        <translation>&amp;Оптические диски</translation>
    </message>
    <message>
        <source>Size</source>
        <translation>Размер</translation>
    </message>
    <message>
        <source>&amp;Floppy Images</source>
        <translation>&amp;Гибкие диски</translation>
    </message>
    <message>
        <source>Attached to</source>
        <comment>VMM: Virtual Disk</comment>
        <translation type="obsolete">Подсоединён к</translation>
    </message>
    <message>
        <source>Attached to</source>
        <comment>VMM: CD/DVD Image</comment>
        <translation type="obsolete">Подсоединён к</translation>
    </message>
    <message>
        <source>Attached to</source>
        <comment>VMM: Floppy Image</comment>
        <translation type="obsolete">Подсоединён к</translation>
    </message>
    <message>
        <source>CD/DVD-ROM disk</source>
        <translation>оптических дисков</translation>
    </message>
    <message>
        <source>hard disk</source>
        <translation>жёстких дисков</translation>
    </message>
    <message>
        <source>floppy disk</source>
        <translation>гибких дисков</translation>
    </message>
    <message>
        <source>All %1 images (%2)</source>
        <translation>Все образы %1 (%2)</translation>
    </message>
    <message>
        <source>Type:</source>
        <translation>Тип:</translation>
    </message>
    <message>
        <source>Location:</source>
        <translation>Расположение:</translation>
    </message>
    <message>
        <source>Format:</source>
        <translation>Формат:</translation>
    </message>
    <message>
        <source>Storage details:</source>
        <translation>Дополнительно:</translation>
    </message>
    <message>
        <source>Attached to:</source>
        <translation>Подсоединён к:</translation>
    </message>
    <message>
        <source>&amp;Copy...</source>
        <translation>&amp;Копировать...</translation>
    </message>
    <message>
        <source>&amp;Modify...</source>
        <translation>&amp;Изменить...</translation>
    </message>
    <message>
        <source>Copy an existing medium</source>
        <translation>Копировать существующий виртуальный носитель</translation>
    </message>
    <message>
        <source>Modify the attributes of the selected medium</source>
        <translation>Изменить атрибуты выбранного виртуального носителя</translation>
    </message>
    <message>
        <source>C&amp;lose</source>
        <translation>&amp;Закрыть</translation>
    </message>
</context>
<context>
    <name>VBoxMiniToolBar</name>
    <message>
        <source>Always show the toolbar</source>
        <translation>Держать тулбар в зоне видимости</translation>
    </message>
    <message>
        <source>Exit Full Screen or Seamless Mode</source>
        <translation>Выйти из режима полного экрана</translation>
    </message>
    <message>
        <source>Close VM</source>
        <translation>Закрыть ВМ</translation>
    </message>
    <message>
        <source>Minimize Window</source>
        <translation>Свернуть окно</translation>
    </message>
</context>
<context>
    <name>VBoxNIList</name>
    <message>
        <source>VirtualBox Host Interface %1</source>
        <translation type="obsolete">Хост-интерфейс VirtualBox %1</translation>
    </message>
    <message>
        <source>&lt;p&gt;Do you want to remove the selected host network interface &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;?&lt;/nobr&gt;&lt;/p&gt;&lt;p&gt;&lt;b&gt;Note:&lt;/b&gt; This interface may be in use by one or more network adapters of this or another VM. After it is removed, these adapters will no longer work until you correct their settings by either choosing a different interface name or a different adapter attachment type.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Хотите ли Вы удалить выбранный хост-интерфейс &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;?&lt;/nobr&gt;&lt;/p&gt;&lt;p&gt;&lt;b&gt;Примечание:&lt;/b&gt; Этот интерфейс может использоваться другими сетевыми адаптерами этой или другой ВМ. После его удаления такие адаптеры не будут работать, пока Вы не исправите их настройки выбором другого хост-интерфейса или изменением типа подсоединения адаптера.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Host &amp;Interfaces</source>
        <translation type="obsolete">&amp;Хост-интерфейсы</translation>
    </message>
    <message>
        <source>Lists all available host interfaces.</source>
        <translation type="obsolete">Перечисляет все имеющиеся хост-интерфейсы.</translation>
    </message>
    <message>
        <source>A&amp;dd New Host Interface</source>
        <translation type="obsolete">&amp;Добавить хост-интерфейс</translation>
    </message>
    <message>
        <source>&amp;Remove Selected Host Interface</source>
        <translation type="obsolete">&amp;Удалить выбранный хост-интерфейс</translation>
    </message>
    <message>
        <source>Adds a new host interface.</source>
        <translation type="obsolete">Добавляет новый хост-интерфейс.</translation>
    </message>
    <message>
        <source>Removes the selected host interface.</source>
        <translation type="obsolete">Удаляет выбранный хост-интерфейс.</translation>
    </message>
</context>
<context>
    <name>VBoxNetworkDialog</name>
    <message>
        <source>Network Adapters</source>
        <translation type="obsolete">Сетевые адаптеры</translation>
    </message>
</context>
<context>
    <name>VBoxOSTypeSelectorWidget</name>
    <message>
        <source>Operating &amp;System:</source>
        <translation>&amp;Операционная система:</translation>
    </message>
    <message>
        <source>Displays the operating system family that you plan to install into this virtual machine.</source>
        <translation>Задает разновидность операционной системы, которую вы хотите установить на эту виртуальную машину.</translation>
    </message>
    <message>
        <source>V&amp;ersion:</source>
        <translation type="obsolete">&amp;Версия:</translation>
    </message>
    <message>
        <source>Displays the operating system type that you plan to install into this virtual machine (called a guest operating system).</source>
        <translation>Задает версию операционной системы, которую вы хотите установить на эту виртуальную машину (эта операционная система называется &quot;гостевая ОС&quot;).</translation>
    </message>
    <message>
        <source>&amp;Version:</source>
        <translation>&amp;Версия:</translation>
    </message>
</context>
<context>
    <name>VBoxProblemReporter</name>
    <message>
        <source>VirtualBox - Information</source>
        <comment>msg box title</comment>
        <translation>VirtualBox - Информация</translation>
    </message>
    <message>
        <source>VirtualBox - Question</source>
        <comment>msg box title</comment>
        <translation>VirtualBox - Вопрос</translation>
    </message>
    <message>
        <source>VirtualBox - Warning</source>
        <comment>msg box title</comment>
        <translation>VirtualBox - Предупреждение</translation>
    </message>
    <message>
        <source>VirtualBox - Error</source>
        <comment>msg box title</comment>
        <translation>VirtualBox - Ошибка</translation>
    </message>
    <message>
        <source>VirtualBox - Critical Error</source>
        <comment>msg box title</comment>
        <translation>VirtualBox - Критическая ошибка</translation>
    </message>
    <message>
        <source>Do not show this message again</source>
        <comment>msg box flag</comment>
        <translation>Больше не показывать это сообщение</translation>
    </message>
    <message>
        <source>Failed to open &lt;tt&gt;%1&lt;/tt&gt;. Make sure your desktop environment can properly handle URLs of this type.</source>
        <translation>Не удалось открыть &lt;tt&gt;%1&lt;/tt&gt;. Убедитесь, что среда Вашего рабочего стола может правильно работать с URL этого типа.</translation>
    </message>
    <message>
        <source>&lt;p&gt;Failed to initialize COM or to find the VirtualBox COM server. Most likely, the VirtualBox server is not running or failed to start.&lt;/p&gt;&lt;p&gt;The application will now terminate.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Не удалось инициализировать подсистему COM или найти COM-сервер программы VirtualBox. Скорее всего, сервер VirtualBox не был запущен или ему не удалось стартовать без ошибок.&lt;/p&gt;&lt;p&gt;Работа приложения будет завершена.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Failed to create the VirtualBox COM object.&lt;/p&gt;&lt;p&gt;The application will now terminate.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Не удалось создать COM-объект VirtualBox.&lt;/p&gt;&lt;p&gt;Работа приложения будет завершена.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Failed to set global VirtualBox properties.</source>
        <translation>Не удалось задать глобальные настройки VirtualBox.</translation>
    </message>
    <message>
        <source>Failed to access the USB subsystem.</source>
        <translation>Не удалось получить доступ к USB-подсистеме.</translation>
    </message>
    <message>
        <source>Failed to create a new virtual machine.</source>
        <translation>Не удалось создать новую виртуальную машину.</translation>
    </message>
    <message>
        <source>Failed to create a new virtual machine &lt;b&gt;%1&lt;/b&gt;.</source>
        <translation>Не удалось создать новую виртуальную машину &lt;b&gt;%1&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Failed to apply the settings to the virtual machine &lt;b&gt;%1&lt;/b&gt;.</source>
        <translation>Не удалось применить настройки виртуальной машины &lt;b&gt;%1&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Failed to start the virtual machine &lt;b&gt;%1&lt;/b&gt;.</source>
        <translation>Не удалось запустить виртуальную машину &lt;b&gt;%1&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Failed to pause the execution of the virtual machine &lt;b&gt;%1&lt;/b&gt;.</source>
        <translation>Не удалось приостановить работу виртуальной машины &lt;b&gt;%1&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Failed to resume the execution of the virtual machine &lt;b&gt;%1&lt;/b&gt;.</source>
        <translation>Не удалось возобновить работу виртуальной машины &lt;b&gt;%1&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Failed to save the state of the virtual machine &lt;b&gt;%1&lt;/b&gt;.</source>
        <translation>Не удалось сохранить состояние виртуальной машины &lt;b&gt;%1&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Failed to create a snapshot of the virtual machine &lt;b&gt;%1&lt;/b&gt;.</source>
        <translation>Не удалось создать снимок виртуальной машины &lt;b&gt;%1&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Failed to stop the virtual machine &lt;b&gt;%1&lt;/b&gt;.</source>
        <translation>Не удалось остановить виртуальную машину &lt;b&gt;%1&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Failed to remove the virtual machine &lt;b&gt;%1&lt;/b&gt;.</source>
        <translation>Не удалось удалить виртуальную машину &lt;b&gt;%1&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Failed to discard the saved state of the virtual machine &lt;b&gt;%1&lt;/b&gt;.</source>
        <translation>Не удалось сбросить сохраненное состояние виртуальной машины &lt;b&gt;%1&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Failed to discard the snapshot &lt;b&gt;%1&lt;/b&gt; of the virtual machine &lt;b&gt;%2&lt;/b&gt;.</source>
        <translation type="obsolete">Не удалось удалить снимок &lt;b&gt;%1&lt;/b&gt; виртуальной машины &lt;b&gt;%2&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Failed to discard the current state of the virtual machine &lt;b&gt;%1&lt;/b&gt;.</source>
        <translation type="obsolete">Не удалось удалить текущее состояние виртуальной машины &lt;b&gt;%1&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Failed to discard the current snapshot and the current state of the virtual machine &lt;b&gt;%1&lt;/b&gt;.</source>
        <translation type="obsolete">Не удалось удалить текущий снимок и текущее состояние виртуальной машины &lt;b&gt;%1&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>There is no virtual machine named &lt;b&gt;%1&lt;/b&gt;.</source>
        <translation>Нет виртуальной машины с именем &lt;b&gt;%1&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>&lt;p&gt;Are you sure you want to permanently delete the virtual machine &lt;b&gt;%1&lt;/b&gt;?&lt;/p&gt;&lt;p&gt;This operation cannot be undone.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Вы уверены, что хотите полностью удалить виртуальную машину &lt;b&gt;%1&lt;/b&gt;?&lt;/p&gt;&lt;p&gt;Эту операцию отменить нельзя.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Are you sure you want to unregister the inaccessible virtual machine &lt;b&gt;%1&lt;/b&gt;?&lt;/p&gt;&lt;p&gt;You will not be able to register it again from GUI.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Вы уверены, что хотите удалить недоступную виртуальную машину &lt;b&gt;%1&lt;/b&gt; из списка зарегистрированных машин?&lt;/p&gt;&lt;p&gt;Вы не сможете зарегистрировать ее вновь средствами графического интерфейса.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Are you sure you want to discard the saved state of the virtual machine &lt;b&gt;%1&lt;/b&gt;?&lt;/p&gt;&lt;p&gt;This operation is equivalent to resetting or powering off the machine without doing a proper shutdown of the guest OS.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Вы уверены, что хотите сбросить (удалить) сохраненное состояние виртуальной машины &lt;b&gt;%1&lt;/b&gt;?&lt;/p&gt;&lt;p&gt;Эта операция равносильна перезапуску или выключению питания машины без надлежащей остановки средствами гостевой ОС.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Failed to create a new session.</source>
        <translation>Не удалось создать новую сессию.</translation>
    </message>
    <message>
        <source>Failed to open a session for the virtual machine &lt;b&gt;%1&lt;/b&gt;.</source>
        <translation>Не удалось открыть сессию для виртуальной машины &lt;b&gt;%1&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Failed to create the host network interface &lt;b&gt;%1&lt;/b&gt;.</source>
        <translation type="obsolete">Не удалось создать сетевой хост-интерфейс &lt;b&gt;%1&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Failed to remove the host network interface &lt;b&gt;%1&lt;/b&gt;.</source>
        <translation>Не удалось удалить сетевой хост-интерфейс &lt;b&gt;%1&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Failed to attach the USB device &lt;b&gt;%1&lt;/b&gt; to the virtual machine &lt;b&gt;%2&lt;/b&gt;.</source>
        <translation>Не удалось подсоединить USB-устройство &lt;b&gt;%1&lt;/b&gt; к виртуальной машине &lt;b&gt;%2&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Failed to detach the USB device &lt;b&gt;%1&lt;/b&gt; from the virtual machine &lt;b&gt;%2&lt;/b&gt;.</source>
        <translation>Не удалось отсоединить USB-устройство &lt;b&gt;%1&lt;/b&gt; от виртуальной машины &lt;b&gt;%2&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Failed to create the shared folder &lt;b&gt;%1&lt;/b&gt; (pointing to &lt;nobr&gt;&lt;b&gt;%2&lt;/b&gt;&lt;/nobr&gt;) for the virtual machine &lt;b&gt;%3&lt;/b&gt;.</source>
        <translation>Не удалось создать общую папку &lt;b&gt;%1&lt;/b&gt; (указывающую на &lt;nobr&gt;&lt;b&gt;%2&lt;/b&gt;&lt;/nobr&gt;) для виртуальной машины &lt;b&gt;%3&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Failed to remove the shared folder &lt;b&gt;%1&lt;/b&gt; (pointing to &lt;nobr&gt;&lt;b&gt;%2&lt;/b&gt;&lt;/nobr&gt;) from the virtual machine &lt;b&gt;%3&lt;/b&gt;.</source>
        <translation>Не удалось удалить общую папку &lt;b&gt;%1&lt;/b&gt; (указывающую на  &lt;nobr&gt;&lt;b&gt;%2&lt;/b&gt;&lt;/nobr&gt;) из виртуальной машины &lt;b&gt;%3&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>&lt;p&gt;The Virtual Machine reports that the guest OS does not support &lt;b&gt;mouse pointer integration&lt;/b&gt; in the current video mode. You need to capture the mouse (by clicking over the VM display or pressing the host key) in order to use the mouse inside the guest OS.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Виртуальная машина сообщает, гостевая ОС не поддерживает &lt;b&gt;интеграцию указателя мыши&lt;/b&gt; в текущем видеорежиме. Чтобы использовать мышь в гостевой ОС, нужно захватить мышь (щелкнув кнопкой мыши в пределах экрана ВМ или нажав хост-клавишу).&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;The Virtual Machine is currently in the &lt;b&gt;Paused&lt;/b&gt; state and not able to see any keyboard or mouse input. If you want to continue to work inside the VM, you need to resume it by selecting the corresponding action from the menu bar.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Виртуальная машина находится в соостоянии &lt;b&gt;Приостановлена&lt;/b&gt; и поэтому не принимает события от клавиатуры или мыши. Если Вы хотите продолжить работу в ВМ, Вам необходимо возобновить ее выполнение, выбрав соответствующее действие из меню.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Cannot run VirtualBox in &lt;i&gt;VM Selector&lt;/i&gt; mode due to local restrictions.&lt;/p&gt;&lt;p&gt;The application will now terminate.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Не удается запустить VirtualBox в режиме &lt;i&gt;Окно выбора ВМ&lt;/i&gt; из-за локальных ограничений.&lt;/p&gt;&lt;p&gt;Работа приложения будет завершена.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;Fatal Error&lt;/nobr&gt;</source>
        <comment>runtime error info</comment>
        <translation>&lt;nobr&gt;Фатальная ошибка&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;Non-Fatal Error&lt;/nobr&gt;</source>
        <comment>runtime error info</comment>
        <translation>&lt;nobr&gt;Нефатальная ошибка&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;Warning&lt;/nobr&gt;</source>
        <comment>runtime error info</comment>
        <translation>&lt;nobr&gt;Предупреждение&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>&lt;nobr&gt;Error ID: &lt;/nobr&gt;</source>
        <comment>runtime error info</comment>
        <translation>&lt;nobr&gt;ID ошибки: &lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>Severity: </source>
        <comment>runtime error info</comment>
        <translation>Важность: </translation>
    </message>
    <message>
        <source>&lt;p&gt;A fatal error has occurred during virtual machine execution! The virtual machine will be powered off. Please copy the following error message using the clipboard to help diagnose the problem:&lt;/p&gt;</source>
        <translation>&lt;p&gt;Во время работы виртуальной машины произошла фатальная ошибка! Выиртуальная машина будет выключена. Рекомендуется скопировать в буфер обмена следующее сообщение об ошибке для дальнейшего анализа:&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;An error has occurred during virtual machine execution! The error details are shown below. You may try to correct the error and resume the virtual machine execution.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Во время работы виртуальной машины произошла ошибка! Подробности ошибки приводятся ниже. Вы можете попытаться исправить ситуацию и возобновить работу виртуальной машины.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;The virtual machine execution may run into an error condition as described below. We suggest that you take an appropriate action to avert the error.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Работа виртуальной машины может привести к возникновению ошибки, описываемой ниже. Вы можете игнорировать это сообщение, но рекомендуется выполнить соответствующие действия для предотвращения возникновения описанной ошибки.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Result&amp;nbsp;Code: </source>
        <comment>error info</comment>
        <translation>Код&amp;nbsp;ошибки: </translation>
    </message>
    <message>
        <source>Component: </source>
        <comment>error info</comment>
        <translation>Компонент: </translation>
    </message>
    <message>
        <source>Interface: </source>
        <comment>error info</comment>
        <translation>Интерфейс: </translation>
    </message>
    <message>
        <source>Callee: </source>
        <comment>error info</comment>
        <translation>Вызванный интерфейс: </translation>
    </message>
    <message>
        <source>Callee&amp;nbsp;RC: </source>
        <comment>error info</comment>
        <translation>Код&amp;nbsp;ошибки&amp;nbsp;метода: </translation>
    </message>
    <message>
        <source>&lt;p&gt;Could not find a language file for the language &lt;b&gt;%1&lt;/b&gt; in the directory &lt;b&gt;&lt;nobr&gt;%2&lt;/nobr&gt;&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;The language will be temporarily reset to the system default language. Please go to the &lt;b&gt;Preferences&lt;/b&gt; dialog which you can open from the &lt;b&gt;File&lt;/b&gt; menu of the main VirtualBox window, and select one of the existing languages on the &lt;b&gt;Language&lt;/b&gt; page.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Не удалось найти файл локализации для языка &lt;b&gt;%1&lt;/b&gt; в каталоге &lt;b&gt;&lt;nobr&gt;%2&lt;/nobr&gt;&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Будет временно установлен системный язык по умолчанию. Перейдите в диалог &lt;b&gt;Настройки&lt;/b&gt;, который можно открыть из меню &lt;b&gt;Файл&lt;/b&gt; главного окна VirtualBox, и выберите один из существующих языков на странице &lt;b&gt;Язык&lt;/b&gt;.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Could not load the language file &lt;b&gt;&lt;nobr&gt;%1&lt;/nobr&gt;&lt;/b&gt;. &lt;p&gt;The language will be temporarily reset to English (built-in). Please go to the &lt;b&gt;Preferences&lt;/b&gt; dialog which you can open from the &lt;b&gt;File&lt;/b&gt; menu of the main VirtualBox window, and select one of the existing languages on the &lt;b&gt;Language&lt;/b&gt; page.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Не удалось загрузить файл локализации &lt;b&gt;&lt;nobr&gt;%1&lt;/nobr&gt;&lt;/b&gt;. &lt;p&gt;Будет временно установлен английский язык (встроенный). Перейдите в диалог &lt;b&gt;Настройки&lt;/b&gt;, который можно открыть из меню &lt;b&gt;Файл&lt;/b&gt; главного окна VirtualBox, и выберите один из существующих языков на странице &lt;b&gt;Язык&lt;/b&gt;.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;The VirtualBox Guest Additions installed in the Guest OS are too old: the installed version is %1, the expected version is %2. Some features that require Guest Additions (mouse integration, guest display auto-resize) will most likely stop working properly.&lt;/p&gt;&lt;p&gt;Please update the Guest Additions to the current version by choosing &lt;b&gt;Install Guest Additions&lt;/b&gt; from the &lt;b&gt;Devices&lt;/b&gt; menu.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;В гостевой ОС обнаружен слишком старый пакет Дополнений гостевой ОС: установлена версия %1, ожидается версия %2. Некоторые возможности, требующие Дополнений гостевой ОС (интеграция мыши, авто-размер экрана гостевой ОС), скорее всего, перестанут функционировать.&lt;/p&gt;&lt;p&gt;Пожалуйста, обновите Дополнения гостевой ОС до текущей версии, выбрав пункт &lt;b&gt;Установить Дополнения гостевой ОС&lt;/b&gt; в меню &lt;b&gt;Устройства&lt;/b&gt;.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;The VirtualBox Guest Additions installed in the Guest OS are outdated: the installed version is %1, the expected version is %2. Some features that require Guest Additions (mouse integration, guest display auto-resize) may not work as expected.&lt;/p&gt;&lt;p&gt;It is recommended to update the Guest Additions to the current version  by choosing &lt;b&gt;Install Guest Additions&lt;/b&gt; from the &lt;b&gt;Devices&lt;/b&gt; menu.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;В гостевой ОС обнаружен устаревший пакет Дополнений гостевой ОС: установлена версия %1, ожидается версия %2. Некоторые возможности, требующие Дополнений гостевой ОС (интеграция мыши, авто-размер экрана гостевой ОС), могут перестать функционировать.&lt;/p&gt;&lt;p&gt;Рекомендуется обновить Дополнения гостевой ОС до текущей версии, выбрав пункт &lt;b&gt;Установить Дополнения гостевой ОС&lt;/b&gt; в меню &lt;b&gt;Устройства&lt;/b&gt;.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;The VirtualBox Guest Additions installed in the Guest OS are too recent for this version of VirtualBox: the installed version is %1, the expected version is %2.&lt;/p&gt;&lt;p&gt;Using a newer version of Additions with an older version of VirtualBox is not supported. Please install the current version of the Guest Additions by choosing &lt;b&gt;Install Guest Additions&lt;/b&gt; from the &lt;b&gt;Devices&lt;/b&gt; menu.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;В гостевой ОС обнаружен слишком новый пакет Дополнений гостевой ОС: установлена версия %1, ожидается версия %2.&lt;/p&gt;&lt;p&gt;Использованее более новой версии Дополнений с более старой версией VirtualBox не поддерживается. Пожалуйста, установите подходящую версию Дополнений гостевой ОС, выбрав пункт &lt;b&gt;Установить Дополнения гостевой ОС&lt;/b&gt; в меню &lt;b&gt;Устройства&lt;/b&gt;.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Failed to change the snapshot folder path of the virtual machine &lt;b&gt;%1&lt;b&gt; to &lt;nobr&gt;&lt;b&gt;%2&lt;/b&gt;&lt;/nobr&gt;.</source>
        <translation type="obsolete">Не удалось измениь путь к папке снимков виртуальной машины &lt;b&gt;%1&lt;b&gt; на &lt;nobr&gt;&lt;b&gt;%2&lt;/b&gt;&lt;/nobr&gt;.</translation>
    </message>
    <message>
        <source>&lt;p&gt;Failed to remove the shared folder &lt;b&gt;%1&lt;/b&gt; (pointing to &lt;nobr&gt;&lt;b&gt;%2&lt;/b&gt;&lt;/nobr&gt;) from the virtual machine &lt;b&gt;%3&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Please close all programs in the guest OS that may be using this shared folder and try again.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Не удалось удалить общую папку &lt;b&gt;%1&lt;/b&gt; (указывающую на &lt;nobr&gt;&lt;b&gt;%2&lt;/b&gt;&lt;/nobr&gt;) из виртуальной машины &lt;b&gt;%3&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Закройте все программы гостевой ОС, которые могут использовать эту папку, и попробуйте снова.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Could not find the VirtualBox Guest Additions CD image file &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/nobr&gt; or &lt;nobr&gt;&lt;b&gt;%2&lt;/b&gt;.&lt;/nobr&gt;&lt;/p&gt;&lt;p&gt;Do you wish to download this CD image from the Internet?&lt;/p&gt;</source>
        <translation>&lt;p&gt;Не удалось найти файл CD-образа Дополнений гостевой ОС &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/nobr&gt; или &lt;nobr&gt;&lt;b&gt;%2&lt;/b&gt;.&lt;/nobr&gt;&lt;/p&gt;&lt;p&gt;Хотите ли Вы скачать этот файл из Интернета?&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Failed to download the VirtualBox Guest Additions CD image from &lt;nobr&gt;&lt;a href=&quot;%1&quot;&gt;%2&lt;/a&gt;.&lt;/nobr&gt;&lt;/p&gt;&lt;p&gt;%3&lt;/p&gt;</source>
        <translation>&lt;p&gt;Не удалось скачать CD-образ Дополнений гостевой ОС по ссылке &lt;nobr&gt;&lt;a href=&quot;%1&quot;&gt;%2&lt;/a&gt;.&lt;/nobr&gt;&lt;/p&gt;&lt;p&gt;%3&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Are you sure you want to download the VirtualBox Guest Additions CD image from &lt;nobr&gt;&lt;a href=&quot;%1&quot;&gt;%2&lt;/a&gt;&lt;/nobr&gt; (size %3 bytes)?&lt;/p&gt;</source>
        <translation>&lt;p&gt;Вы уверены, что хотите скачать CD-образ Дополнений гостевой ОС по ссылке&lt;nobr&gt;&lt;a href=&quot;%1&quot;&gt;%2&lt;/a&gt;&lt;/nobr&gt; (размер %3 байт)?&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;The VirtualBox Guest Additions CD image has been successfully downloaded from &lt;nobr&gt;&lt;a href=&quot;%1&quot;&gt;%2&lt;/a&gt;&lt;/nobr&gt; and saved locally as &lt;nobr&gt;&lt;b&gt;%3&lt;/b&gt;.&lt;/nobr&gt;&lt;/p&gt;&lt;p&gt;Do you wish to register this CD image and mount it on the virtual CD/DVD drive?&lt;/p&gt;</source>
        <translation>&lt;p&gt;CD-образ Дополнений гостевой ОС был успешно скачан по ссылке &lt;nobr&gt;&lt;a href=&quot;%1&quot;&gt;%2&lt;/a&gt;&lt;/nobr&gt; и сохранен локально как &lt;nobr&gt;&lt;b&gt;%3&lt;/b&gt;.&lt;/nobr&gt;&lt;/p&gt;&lt;p&gt;Хотите ли Вы зарегистрировать этот CD-образ и подключить его к виртуальному CD/DVD-приводу?&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;The virtual machine window is optimized to work in &lt;b&gt;%1&amp;nbsp;bit&lt;/b&gt; color mode but the virtual display is currently set to &lt;b&gt;%2&amp;nbsp;bit&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Please open the display properties dialog of the guest OS and select a &lt;b&gt;%3&amp;nbsp;bit&lt;/b&gt; color mode, if it is available, for best possible performance of the virtual video subsystem.&lt;/p&gt;&lt;p&gt;&lt;b&gt;Note&lt;/b&gt;. Some operating systems, like OS/2, may actually work in 32&amp;nbsp;bit mode but report it as 24&amp;nbsp;bit (16 million colors). You may try to select a different color mode to see if this message disappears or you can simply disable the message now if you are sure the required color mode (%4&amp;nbsp;bit) is not available in the guest OS.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Окно виртуальной машины оптимизировано для работы в режиме &lt;b&gt;%1-битной&lt;/b&gt; цветопередачи, однако в настоящий момент виртуальный дисплей работает в &lt;b&gt;%2-битном&lt;/b&gt; режиме.&lt;/p&gt;&lt;p&gt;Откройте диалог свойств дисплея гостевой ОС и выберите &lt;b&gt;%3-битный&lt;/b&gt; режим цветопередачи (если он доступен) для обеспечения наилучшей производительности виртуальной видеоподсистемы.&lt;/p&gt;&lt;p&gt;&lt;b&gt;Примечание&lt;/b&gt;. Некоторые операционные системы, такие как OS/2, могут фактически работать в 32-битном режиме, но показывать его в настройках как 24-битный (16 миллионов цветов). Вы можете попробовать выбрать другой режим цветопередачи, чтобы проверить, не исчезнет ли это сообщение. Помимо этого, Вы можете запретить показ данного сообщения, если Вы уверены, что требуемый режим цветопередачи (%4&amp;nbsp;бит) недоступен в данной гостевой ОС.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;You didn&apos;t attach a hard disk to the new virtual machine. The machine will not be able to boot unless you attach a hard disk with a guest operating system or some other bootable media to it later using the machine settings dialog or the First Run Wizard.&lt;/p&gt;&lt;p&gt;Do you wish to continue?&lt;/p&gt;</source>
        <translation>&lt;p&gt;Вы не подсоединили ни одного жесткого диска к новой виртуальной машине. Такая машина не сможет загрузиться, пока Вы не подсоедините к ней жесткий диск с гостевой операционной системой или какой-либо другой загрузочный носитель позже, используя диалог свойств машины или Мастер первого запуска.&lt;/p&gt;&lt;p&gt;Хотите ли Вы продолжить?&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Failed to find license files in &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/nobr&gt;.</source>
        <translation>Не удалось найти файлы лицензий в папке &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/nobr&gt;.</translation>
    </message>
    <message>
        <source>Failed to open the license file &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/nobr&gt;. Check file permissions.</source>
        <translation>Не удалось открыть файл лицензии &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/nobr&gt;. Проверьте права доступа к файлу.</translation>
    </message>
    <message>
        <source>Failed to send the ACPI Power Button press event to the virtual machine &lt;b&gt;%1&lt;/b&gt;.</source>
        <translation>Не удалось послать ACPI-сигнал нажатия кнопки питания в виртуальную машину &lt;b&gt;%1&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>&lt;p&gt;Congratulations! You have been successfully registered as a user of VirtualBox.&lt;/p&gt;&lt;p&gt;Thank you for finding time to fill out the registration form!&lt;/p&gt;</source>
        <translation>&lt;p&gt;Поздравляем! Вы успешно зарегистрировались в качестве пользователя VirtualBox.&lt;/p&gt;&lt;p&gt;Спасибо за то, что нашли время и заполнили эту регистрационную форму!&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Failed to register the VirtualBox product&lt;/p&gt;&lt;p&gt;%1&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Не удалось зарегистрировать продукт VirtualBox&lt;/p&gt;&lt;p&gt;%1&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Failed to save the global VirtualBox settings to &lt;b&gt;&lt;nobr&gt;%1&lt;/nobr&gt;&lt;/b&gt;.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Не удалось сохранить глобальные настройки VirtualBox в &lt;b&gt;&lt;nobr&gt;%1&lt;/nobr&gt;&lt;/b&gt;.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Failed to load the global GUI configuration from &lt;b&gt;&lt;nobr&gt;%1&lt;/nobr&gt;&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;The application will now terminate.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Не удалось загрузить глобальные настройки интерфейса из &lt;b&gt;&lt;nobr&gt;%1&lt;/nobr&gt;&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Работа приложения будет завершена.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Failed to save the global GUI configuration to &lt;b&gt;&lt;nobr&gt;%1&lt;/nobr&gt;&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;The application will now terminate.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Не удалось сохранить глобальные настройки интерфейса в &lt;b&gt;&lt;nobr&gt;%1&lt;/nobr&gt;&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Работа приложения будет завершена.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Failed to save the settings of the virtual machine &lt;b&gt;%1&lt;/b&gt; to &lt;b&gt;&lt;nobr&gt;%2&lt;/nobr&gt;&lt;/b&gt;.</source>
        <translation>Не удалось сохранить настройки виртуальной машины &lt;b&gt;%1&lt;/b&gt; в &lt;b&gt;&lt;nobr&gt;%2&lt;/nobr&gt;&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Failed to load the settings of the virtual machine &lt;b&gt;%1&lt;/b&gt; from &lt;b&gt;&lt;nobr&gt;%2&lt;/nobr&gt;&lt;/b&gt;.</source>
        <translation>Не удалось загрузить настройки виртуальной машины &lt;b&gt;%1&lt;/b&gt; из &lt;b&gt;&lt;nobr&gt;%2&lt;/nobr&gt;&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Delete</source>
        <comment>machine</comment>
        <translation type="obsolete">Удалить</translation>
    </message>
    <message>
        <source>Unregister</source>
        <comment>machine</comment>
        <translation type="obsolete">Дерегистрировать</translation>
    </message>
    <message>
        <source>Discard</source>
        <comment>saved state</comment>
        <translation>Сбросить</translation>
    </message>
    <message>
        <source>&lt;p&gt;There are hard disks attached to SATA ports of this virtual machine. If you disable the SATA controller, all these hard disks will be automatically detached.&lt;/p&gt;&lt;p&gt;Are you sure that you want to disable the SATA controller?&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;К SATA-портам этой виртуальной машины подключены жесткие диски. Если вы отключите контроллер SATA, то все эти жесткие диски будут автоматически отсоединены.&lt;/p&gt;&lt;p&gt;Уверены ли Вы, что хотите отключить контроллер SATA?&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Disable</source>
        <comment>hard disk</comment>
        <translation type="obsolete">Отключить</translation>
    </message>
    <message>
        <source>Download</source>
        <comment>additions</comment>
        <translation>Скачать</translation>
    </message>
    <message>
        <source>Mount</source>
        <comment>additions</comment>
        <translation>Подключить</translation>
    </message>
    <message>
        <source>&lt;p&gt;The host key is currently defined as &lt;b&gt;%1&lt;/b&gt;.&lt;/p&gt;</source>
        <comment>additional message box paragraph</comment>
        <translation>&lt;p&gt;В данный момент в качестве хост-клавиши используется &lt;b&gt;%1&lt;/b&gt;.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Capture</source>
        <comment>do input capture</comment>
        <translation>Захватить</translation>
    </message>
    <message>
        <source>Check</source>
        <comment>inaccessible media message box</comment>
        <translation>Проверить</translation>
    </message>
    <message>
        <source>&amp;Backup</source>
        <comment>warnAboutAutoConvertedSettings message box</comment>
        <translation type="obsolete">&amp;Копировать</translation>
    </message>
    <message>
        <source>Switch</source>
        <comment>fullscreen</comment>
        <translation>Переключить</translation>
    </message>
    <message>
        <source>Switch</source>
        <comment>seamless</comment>
        <translation>Переключить</translation>
    </message>
    <message>
        <source>&lt;p&gt;Do you really want to reset the virtual machine?&lt;/p&gt;&lt;p&gt;This will cause any unsaved data in applications running inside it to be lost.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Вы действительно хотите выполнить перезапуск виртуальной машины?&lt;/p&gt;&lt;p&gt;Во время перезапуска произойдет утеря несохраненных данных всех приложений, работающих внутри виртуальной машины.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Reset</source>
        <comment>machine</comment>
        <translation>Перезапуск</translation>
    </message>
    <message>
        <source>Continue</source>
        <comment>no hard disk attached</comment>
        <translation>Продолжить</translation>
    </message>
    <message>
        <source>Go Back</source>
        <comment>no hard disk attached</comment>
        <translation>Назад</translation>
    </message>
    <message>
        <source>Failed to copy file &lt;b&gt;&lt;nobr&gt;%1&lt;/nobr&gt;&lt;/b&gt; to &lt;b&gt;&lt;nobr&gt;%2&lt;/nobr&gt;&lt;/b&gt; (%3).</source>
        <translation type="obsolete">Не удалось скопировать файл &lt;b&gt;&lt;nobr&gt;%1&lt;/nobr&gt;&lt;/b&gt; в &lt;b&gt;&lt;nobr&gt;%2&lt;/nobr&gt;&lt;/b&gt; (%3).</translation>
    </message>
    <message>
        <source>&amp;Create</source>
        <comment>hard disk</comment>
        <translation type="obsolete">&amp;Создать</translation>
    </message>
    <message>
        <source>Select</source>
        <comment>hard disk</comment>
        <translation type="obsolete">Выбрать</translation>
    </message>
    <message>
        <source>&lt;p&gt;Could not enter seamless mode due to insufficient guest video memory.&lt;/p&gt;&lt;p&gt;You should configure the virtual machine to have at least &lt;b&gt;%1&lt;/b&gt; of video memory.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Не удалось переключиться в режим интеграции дисплея из-за недостаточного количества виртуальной видеопамяти.&lt;/p&gt;&lt;p&gt;Необходимо задать как минимум &lt;b&gt;%1&lt;/b&gt; видеопамяти в диалоге свойств виртаульной машины.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Could not switch the guest display to fullscreen mode due to insufficient guest video memory.&lt;/p&gt;&lt;p&gt;You should configure the virtual machine to have at least &lt;b&gt;%1&lt;/b&gt; of video memory.&lt;/p&gt;&lt;p&gt;Press &lt;b&gt;Ignore&lt;/b&gt; to switch to fullscreen mode anyway or press &lt;b&gt;Cancel&lt;/b&gt; to cancel the operation.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Не удалось переключить дисплей гостевой ОС в полноэкранный режим из-за недостаточного количества виртуальной видеопамяти.&lt;/p&gt;&lt;p&gt;Необходимо задать как минимум &lt;b&gt;%1&lt;/b&gt; видеопамяти в диалоге свойств виртаульной машины.&lt;/p&gt;&lt;p&gt;Нажмите &lt;b&gt;Игнорировать&lt;/b&gt;, чтобы переключиться в полноэкранный режим в любом случае, или нажмите &lt;b&gt;Отмена&lt;/b&gt; для отмены операции.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>You are already running the most recent version of VirtualBox.</source>
        <translation>Вы уже установили последнюю версию программы VirtualBox. Повторите проверку обновлений позже.</translation>
    </message>
    <message>
        <source>&lt;p&gt;You have &lt;b&gt;clicked the mouse&lt;/b&gt; inside the Virtual Machine display or pressed the &lt;b&gt;host key&lt;/b&gt;. This will cause the Virtual Machine to &lt;b&gt;capture&lt;/b&gt; the host mouse pointer (only if the mouse pointer integration is not currently supported by the guest OS) and the keyboard, which will make them unavailable to other applications running on your host machine.&lt;/p&gt;&lt;p&gt;You can press the &lt;b&gt;host key&lt;/b&gt; at any time to &lt;b&gt;uncapture&lt;/b&gt; the keyboard and mouse (if it is captured) and return them to normal operation. The currently assigned host key is shown on the status bar at the bottom of the Virtual Machine window, next to the&amp;nbsp;&lt;img src=:/hostkey_16px.png/&gt;&amp;nbsp;icon. This icon, together with the mouse icon placed nearby, indicate the current keyboard and mouse capture state.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Вы &lt;b&gt;щелкнули кнопкой мыши&lt;/b&gt; внутри экрана виртуальной машины или нажали &lt;b&gt;хост-клавишу&lt;/b&gt;. Это приведет к тому, что виртуальная машина &lt;b&gt;захватит&lt;/b&gt; указатель мыши (только в случае, если интеграция указателя мыши не поддерживается гостевой ОС) и клавиатуру основного ПК, что сделает их недоступными для других приложений, работающих на компьютере.&lt;/p&gt;&lt;p&gt;Вы можете нажать &lt;b&gt;хост-клавишу&lt;/b&gt; в любое время, чтобы &lt;b&gt;освободить&lt;/b&gt; клавиатуру и мышь (если они захвачены) и вернуть их к нормальной работе. Текущая хост-клавиша отображается в строке состояния внизу окна виртуальной машины, рядом со значком &amp;nbsp;&lt;img src=:/hostkey_16px.png/&gt;&amp;nbsp;. Этот значок, а также значок с изображением мыши, расположенный рядом, показывают текущее состояние захвата клавиатуры и мыши.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;You have the &lt;b&gt;Auto capture keyboard&lt;/b&gt; option turned on. This will cause the Virtual Machine to automatically &lt;b&gt;capture&lt;/b&gt; the keyboard every time the VM window is activated and make it unavailable to other applications running on your host machine: when the keyboard is captured, all keystrokes (including system ones like Alt-Tab) will be directed to the VM.&lt;/p&gt;&lt;p&gt;You can press the &lt;b&gt;host key&lt;/b&gt; at any time to &lt;b&gt;uncapture&lt;/b&gt; the keyboard and mouse (if it is captured) and return them to normal operation. The currently assigned host key is shown on the status bar at the bottom of the Virtual Machine window, next to the&amp;nbsp;&lt;img src=:/hostkey_16px.png/&gt;&amp;nbsp;icon. This icon, together with the mouse icon placed nearby, indicate the current keyboard and mouse capture state.&lt;/p&gt;</source>
        <translation>&lt;p&gt;У Вас включена настройка &lt;b&gt;Автозахват клавиатуры&lt;/b&gt;. Это приведет к тому, что виртуальная машина будет автоматически &lt;b&gt;захватывать&lt;/b&gt; клавиатуру каждый раз при переключении в окно ВМ, что сделает ее недоступной для других приложений, работающих на компьютере: когда клавиатура захвачена, все нажатия клавиш (включая системные, такие как Alt-Tab) будут направлены в виртуальную машину.&lt;/p&gt;&lt;p&gt;Вы можете нажать &lt;b&gt;хост-клавишу&lt;/b&gt; в любое время, чтобы &lt;b&gt;освободить&lt;/b&gt; клавиатуру и мышь (если они захвачены) и вернуть их к нормальной работе. Текущая хост-клавиша отображается в строке состояния внизу окна виртуальной машины, рядом со значком &amp;nbsp;&lt;img src=:/hostkey_16px.png/&gt;&amp;nbsp;. Этот значок, а также значок с изображением мыши, расположенный рядом, показывают текущее состояние захвата клавиатуры и мыши.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;The Virtual Machine reports that the guest OS supports &lt;b&gt;mouse pointer integration&lt;/b&gt;. This means that you do not need to &lt;i&gt;capture&lt;/i&gt; the mouse pointer to be able to use it in your guest OS -- all mouse actions you perform when the mouse pointer is over the Virtual Machine&apos;s display are directly sent to the guest OS. If the mouse is currently captured, it will be automatically uncaptured.&lt;/p&gt;&lt;p&gt;The mouse icon on the status bar will look like&amp;nbsp;&lt;img src=:/mouse_seamless_16px.png/&gt;&amp;nbsp;to inform you that mouse pointer integration is supported by the guest OS and is currently turned on.&lt;/p&gt;&lt;p&gt;&lt;b&gt;Note&lt;/b&gt;: Some applications may behave incorrectly in mouse pointer integration mode. You can always disable it for the current session (and enable it again) by selecting the corresponding action from the menu bar.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Виртуальная машина сообщает, что гостевая ОС поддерживает &lt;b&gt;интеграцию указателя мыши&lt;/b&gt;. Это означает, что не требуется &lt;i&gt;захватывать&lt;/i&gt; указатель мыши для того, чтобы использовать ее в гостевой ОС -- все действия с мышью, когда ее указатель находится в пределах экрана виртуальной машины, напрямую передаются в гостевую ОС. Если мышь в настоящий момент захвачена, она будет автоматически освобождена.&lt;/p&gt;&lt;p&gt;Значок мыши в строке состояния будет выглядеть так: &amp;nbsp;&lt;img src=:/mouse_seamless_16px.png/&gt;&amp;nbsp; -- это говорит о том, что интеграция мыши поддерживается гостевой ОС и в настоящий момент включена.&lt;/p&gt;&lt;p&gt;&lt;b&gt;Примечание&lt;/b&gt;: Некоторые приложения могут вести себя неправильно в режиме интеграции указателя мыши. Вы всегда можете отключить этот режим для текущей сессии (и включить его снова), выбрав соответствующее действие из меню.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;The virtual machine window will be now switched to &lt;b&gt;fullscreen&lt;/b&gt; mode. You can go back to windowed mode at any time by pressing &lt;b&gt;%1&lt;/b&gt;. Note that the &lt;i&gt;Host&lt;/i&gt; key is currently defined as &lt;b&gt;%2&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Note that the main menu bar is hidden in fullscreen mode. You can access it by pressing &lt;b&gt;Host+Home&lt;/b&gt;.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Сейчас окно виртуальной машины будет переключено в &lt;b&gt;полноэкранный&lt;/b&gt; режим. Вы можете вернуться в оконный режим в любое время, нажав &lt;b&gt;%1&lt;/b&gt;. Обратите внимание, что в данный момент в качестве &lt;i&gt;хост-клавиши&lt;/i&gt; используется &lt;b&gt;%2&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Имейте в виду, что в полноэкранном режиме основное меню окна скрыто. Вы можете получить к нему доступ, нажав &lt;b&gt;Host+Home&lt;/b&gt;.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;The virtual machine window will be now switched to &lt;b&gt;Seamless&lt;/b&gt; mode. You can go back to windowed mode at any time by pressing &lt;b&gt;%1&lt;/b&gt;. Note that the &lt;i&gt;Host&lt;/i&gt; key is currently defined as &lt;b&gt;%2&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Note that the main menu bar is hidden in seamless mode. You can access it by pressing &lt;b&gt;Host+Home&lt;/b&gt;.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Сейчас окно виртуальной машины будет переключено в режим &lt;b&gt;интеграции дисплея&lt;/b&gt;. Вы можете вернуться в оконный режим в любое время, нажав &lt;b&gt;%1&lt;/b&gt;. Обратите внимание, что в данный момент в качестве &lt;i&gt;хост-клавиши&lt;/i&gt; используется &lt;b&gt;%2&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Имейте в виду, что в режиме интеграции дисплея основное меню окна скрыто. Вы можете получить к нему доступ, нажав &lt;b&gt;Host+Home&lt;/b&gt;.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&amp;Contents...</source>
        <translation>&amp;Содержание...</translation>
    </message>
    <message>
        <source>Show the online help contents</source>
        <translation>Показать содержание оперативной справки</translation>
    </message>
    <message>
        <source>&amp;VirtualBox Web Site...</source>
        <translation>&amp;Веб-страница VirtualBox...</translation>
    </message>
    <message>
        <source>Open the browser and go to the VirtualBox product web site</source>
        <translation>Открыть браузер и перейти на сайт программы VirtualBox</translation>
    </message>
    <message>
        <source>&amp;Reset All Warnings</source>
        <translation>&amp;Разрешить все предупреждения</translation>
    </message>
    <message>
        <source>Go back to showing all suppressed warnings and messages</source>
        <translation>Включить отображение всех отключенных ранее предупреждений и сообщений</translation>
    </message>
    <message>
        <source>R&amp;egister VirtualBox...</source>
        <translation>&amp;Зарегистрировать VirtualBox...</translation>
    </message>
    <message>
        <source>Open VirtualBox registration form</source>
        <translation>Открыть регистрационную форму VirtualBox</translation>
    </message>
    <message>
        <source>C&amp;heck for Updates...</source>
        <translation>&amp;Проверить обновления...</translation>
    </message>
    <message>
        <source>Check for a new VirtualBox version</source>
        <translation>Проверить наличие новой версии VirtualBox через Интернет</translation>
    </message>
    <message>
        <source>&amp;About VirtualBox...</source>
        <translation>&amp;О программе...</translation>
    </message>
    <message>
        <source>Show a dialog with product information</source>
        <translation>Показать диалоговое окно с информацией о программе VirtualBox</translation>
    </message>
    <message>
        <source>&lt;p&gt;A new version of VirtualBox has been released! Version &lt;b&gt;%1&lt;/b&gt; is available at &lt;a href=&quot;http://www.virtualbox.org/&quot;&gt;virtualbox.org&lt;/a&gt;.&lt;/p&gt;&lt;p&gt;You can download this version using the link:&lt;/p&gt;&lt;p&gt;&lt;a href=%2&gt;%3&lt;/a&gt;&lt;/p&gt;</source>
        <translation>&lt;p&gt;Выпущена новая версия программы VirtualBox! Версия &lt;b&gt;%1&lt;/b&gt; доступна на сайте &lt;a href=&quot;http://www.virtualbox.org/&quot;&gt;virtualbox.org&lt;/a&gt;.&lt;/p&gt;&lt;p&gt;Вы можете скачать эту версию, используя следующую прямую ссылку: &lt;/p&gt;&lt;p&gt;&lt;a href=%2&gt;%3&lt;/a&gt;&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Are you sure you want to release the %1 &lt;nobr&gt;&lt;b&gt;%2&lt;/b&gt;&lt;/nobr&gt;?&lt;/p&gt;&lt;p&gt;This will detach it from the following virtual machine(s): &lt;b&gt;%3&lt;/b&gt;.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Вы уверены, что хотите освободить %1 &lt;nobr&gt;&lt;b&gt;%2&lt;/b&gt;&lt;/nobr&gt;?&lt;/p&gt;&lt;p&gt;Это приведет к отсоединению этого носителя от следующих виртуальных машин: &lt;b&gt;%3&lt;/b&gt;.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Release</source>
        <comment>detach medium</comment>
        <translation>Освободить</translation>
    </message>
    <message>
        <source>&lt;p&gt;Are you sure you want to remove the %1 &lt;nobr&gt;&lt;b&gt;%2&lt;/b&gt;&lt;/nobr&gt; from the list of known media?&lt;/p&gt;</source>
        <translation>&lt;p&gt;Вы уверены, что хотите убрать %1 &lt;nobr&gt;&lt;b&gt;%2&lt;/b&gt;&lt;/nobr&gt; из списка используемых носителей?&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Note that as this hard disk is inaccessible its storage unit cannot be deleted right now.</source>
        <translation>Имейте в виду, что этот жесткий диск недоступен, поэтому его файл не может быть удален.</translation>
    </message>
    <message>
        <source>The next dialog will let you choose whether you also want to delete the storage unit of this hard disk or keep it for later usage.</source>
        <translation>Следующий диалог позволит Вам выбрать, нужно ли удалять файл этого жесткого диска или Вы хотите сохранить его для дальнейшего использования.</translation>
    </message>
    <message>
        <source>&lt;p&gt;Note that the storage unit of this medium will not be deleted and that it will be possible to add it to the list later again.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Имейте в виду, что файл с данными этого носителя не будет удален и в дальнейшем может быть снова добавлен в список.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Remove</source>
        <comment>medium</comment>
        <translation>Убрать</translation>
    </message>
    <message>
        <source>&lt;p&gt;The hard disk storage unit at location &lt;b&gt;%1&lt;/b&gt; already exists. You cannot create a new virtual hard disk that uses this location because it can be already used by another virtual hard disk.&lt;/p&gt;&lt;p&gt;Please specify a different location.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Файл жесткого диска &lt;b&gt;%1&lt;/b&gt;, уже существует. Вы не можете создать новый виртуальный жесткий диск, который использует этот файл, потому что он, возможно, уже используется другим жестким диском.&lt;/p&gt;&lt;p&gt;Пожалуйста, укажите другое расположение файла.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Do you want to delete the storage unit of the hard disk &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/nobr&gt;?&lt;/p&gt;&lt;p&gt;If you select &lt;b&gt;Delete&lt;/b&gt; then the specified storage unit will be permanently deleted. This operation &lt;b&gt;cannot be undone&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;If you select &lt;b&gt;Keep&lt;/b&gt; then the hard disk will be only removed from the list of known hard disks, but the storage unit will be left untouched which makes it possible to add this hard disk to the list later again.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Хотите ли Вы удалить файл жесткого диска &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/nobr&gt;?&lt;/p&gt;&lt;p&gt;Если Вы выберете &lt;b&gt;Удалить&lt;/b&gt;, то указанный файл будет физически удален. Эту операцию &lt;b&gt;отменить нельзя&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Если Вы выберете &lt;b&gt;Сохранить&lt;/b&gt;, то жесткий диск будет убран из списка используемых жестких дисков, но указанный файл удален не будет, что дает возможность вновь добавить этот жесткий диск в список при необходимости.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Delete</source>
        <comment>hard disk storage</comment>
        <translation>Удалить</translation>
    </message>
    <message>
        <source>Keep</source>
        <comment>hard disk storage</comment>
        <translation>Сохранить</translation>
    </message>
    <message>
        <source>Failed to delete the storage unit of the hard disk &lt;b&gt;%1&lt;/b&gt;.</source>
        <translation>Не удалось удалить файл жесткого диска &lt;b&gt;%1&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>&lt;p&gt;There are no unused hard disks available for the newly created attachment.&lt;/p&gt;&lt;p&gt;Press the &lt;b&gt;Create&lt;/b&gt; button to start the &lt;i&gt;New Virtual Disk&lt;/i&gt; wizard and create a new hard disk, or press the &lt;b&gt;Select&lt;/b&gt; if you wish to open the &lt;i&gt;Virtual Media Manager&lt;/i&gt;.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Отсутствуют неиспользуемые жесткие диски, доступные для созданного подключения.&lt;/p&gt;&lt;p&gt;Нажмите кнопку &lt;b&gt;Создать&lt;/b&gt; для запуска &lt;i&gt;Мастера нового виртуального диска&lt;/i&gt; и создания нового жесткого диска, либо кнопку &lt;b&gt;Выбрать&lt;/b&gt; для открытия &lt;i&gt;Менеджера виртуальных носителей&lt;/i&gt; и выбора нужного действия.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Failed to create the hard disk storage &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;.&lt;/nobr&gt;</source>
        <translation>Не удалось создать файл жесткого диска &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;.&lt;/nobr&gt;</translation>
    </message>
    <message>
        <source>Failed to attach the hard disk &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/nobr&gt; to slot &lt;i&gt;%2&lt;/i&gt; of the machine &lt;b&gt;%3&lt;/b&gt;.</source>
        <translation type="obsolete">Не удалось подсоединить жесткий диск &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/nobr&gt; к слоту  &lt;i&gt;%2&lt;/i&gt; машины &lt;b&gt;%3&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Failed to detach the hard disk &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/nobr&gt; from slot &lt;i&gt;%2&lt;/i&gt; of the machine &lt;b&gt;%3&lt;/b&gt;.</source>
        <translation type="obsolete">Не удалось отсоединить жесткий диск &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/nobr&gt; от слота &lt;i&gt;%2&lt;/i&gt; машины &lt;b&gt;%3&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Failed to mount the %1 &lt;nobr&gt;&lt;b&gt;%2&lt;/b&gt;&lt;/nobr&gt; on the machine &lt;b&gt;%3&lt;/b&gt;.</source>
        <translation type="obsolete">Не удалось подключить %1 &lt;nobr&gt;&lt;b&gt;%2&lt;/b&gt;&lt;/nobr&gt; к машине &lt;b&gt;%3&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Failed to unmount the %1 &lt;nobr&gt;&lt;b&gt;%2&lt;/b&gt;&lt;/nobr&gt; from the machine &lt;b&gt;%3&lt;/b&gt;.</source>
        <translation type="obsolete">Не удалось отключить %1 &lt;nobr&gt;&lt;b&gt;%2&lt;/b&gt;&lt;/nobr&gt; от машины &lt;b&gt;%3&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Failed to open the %1 &lt;nobr&gt;&lt;b&gt;%2&lt;/b&gt;&lt;/nobr&gt;.</source>
        <translation>Не удалось открыть %1 &lt;nobr&gt;&lt;b&gt;%2&lt;/b&gt;&lt;/nobr&gt;.</translation>
    </message>
    <message>
        <source>Failed to close the %1 &lt;nobr&gt;&lt;b&gt;%2&lt;/b&gt;&lt;/nobr&gt;.</source>
        <translation>Не удалось закрыть %1 &lt;nobr&gt;&lt;b&gt;%2&lt;/b&gt;&lt;/nobr&gt;.</translation>
    </message>
    <message>
        <source>Failed to determine the accessibility state of the medium &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/nobr&gt;.</source>
        <translation>Не удалось проверить доступность носителя &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/nobr&gt;.</translation>
    </message>
    <message>
        <source>&lt;p&gt;Failed to connect to the VirtualBox online registration service due to the following error:&lt;/p&gt;&lt;p&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/p&gt;</source>
        <translation>&lt;p&gt;Не удалось подключится к службе онлайн-регистрации VirtualBox из-за следующей ошибки:&lt;/p&gt;&lt;p&gt;%1&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Unable to obtain the new version information due to the following error:&lt;/p&gt;&lt;p&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/p&gt;</source>
        <translation>&lt;p&gt;Невозможно получить информацию о новой версии из-за следующей ошибки:&lt;/p&gt;&lt;p&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;One or more virtual hard disks, CD/DVD or floppy media are not currently accessible. As a result, you will not be able to operate virtual machines that use these media until they become accessible later.&lt;/p&gt;&lt;p&gt;Press &lt;b&gt;Check&lt;/b&gt; to open the Virtual Media Manager window and see what media are inaccessible, or press &lt;b&gt;Ignore&lt;/b&gt; to ignore this message.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Один или несколько виртуальных жестких дисков, образов CD/DVD или дискет сейчас недоступны. В результате, Вы не сможете запускать виртуальные машины, использующие эти носители, до тех пор, пока к ним не появится доступ.&lt;/p&gt;&lt;p&gt;Нажмите &lt;b&gt;Проверить&lt;/b&gt;, чтобы открыть окно Менеджера виртуальных носителей и увидеть, какие именно носители недоступны, или нажмите &lt;b&gt;Игнорировать&lt;/b&gt;, чтобы пропустить это сообщение.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Your existing VirtualBox settings files were automatically converted from the old format to a new format required by the new version of VirtualBox.&lt;/p&gt;&lt;p&gt;Press &lt;b&gt;OK&lt;/b&gt; to start VirtualBox now or press &lt;b&gt;More&lt;/b&gt; if you want to get more information about what files were converted and access additional actions.&lt;/p&gt;&lt;p&gt;Press &lt;b&gt;Exit&lt;/b&gt; to terminate the VirtualBox application without saving the results of the conversion to disk.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Существующие файлы настроек VirtualBox были автоматически преобразованы из старого формата в формат, который требуется для новой версии VirtualBox.&lt;/p&gt;&lt;p&gt;Нажмите &lt;b&gt;ОК&lt;/b&gt; для запуска VirtualBox или нажмите &lt;b&gt;Дополнительно&lt;/b&gt;, если Вы хотите узнать, какие именно файлы были преобразованы, а также получить доступ к дополнительным действиям.&lt;/p&gt;&lt;p&gt;Нажмите &lt;b&gt;Выход&lt;/b&gt; для завершения приложения без сохранения результатов преобразования на диск.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&amp;More</source>
        <comment>warnAboutAutoConvertedSettings message box</comment>
        <translation type="obsolete">&amp;Дополнительно</translation>
    </message>
    <message>
        <source>E&amp;xit</source>
        <comment>warnAboutAutoConvertedSettings message box</comment>
        <translation type="obsolete">&amp;Выход</translation>
    </message>
    <message>
        <source>&lt;p&gt;The following VirtualBox settings files have been automatically converted to the new settings file format version &lt;b&gt;%1&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;However, the results of the conversion were not saved back to disk yet. Please press:&lt;/p&gt;&lt;ul&gt;&lt;li&gt;&lt;b&gt;Backup&lt;/b&gt; to create backup copies of the settings files in the old format before saving them in the new format;&lt;/li&gt;&lt;li&gt;&lt;b&gt;Overwrite&lt;/b&gt; to save all auto-converted files without creating backup copies (it will not be possible to use these settings files with an older version of VirtualBox afterwards);&lt;/li&gt;%2&lt;/ul&gt;&lt;p&gt;It is recommended to always select &lt;b&gt;Backup&lt;/b&gt; because in this case it will be possible to go back to the previous version of VirtualBox (if necessary) without losing your current settings. See the VirtualBox Manual for more information about downgrading.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Нижеперечисленные файлы настроек VirtualBox были автоматически преобразованы в новый формат версии &lt;b&gt;%1&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Однако, результаты преобразования пока еще не были сохранены на диск. Пожалуйста, нажмите:&lt;/p&gt;&lt;ul&gt;&lt;li&gt;&lt;b&gt;Копировать&lt;/b&gt;, чтобы создать резервные копии файлов настроек в старом формате перед сохранением их в новом формате;&lt;/li&gt;&lt;li&gt;&lt;b&gt;Перезаписать&lt;/b&gt;, чтобы сохранить все преобразованные файлы настроек поверх старых версий без создания резервных копий (Вы больше не сможете использовать эти файлы с предыдущими версиями VirtualBox);&lt;/li&gt;%2&lt;/ul&gt;&lt;p&gt;Рекомендуется всегда выбирать &lt;b&gt;Копировать&lt;/b&gt;, так как это позволит Вам вернуться к предыдущей версии VirtualBox (если возникнет такая необходимость) без потери текущих настроек. Обратитесь к Руководству пользователя VirtualBox за более подробной информацией о возврате к предыдущим версиям.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;li&gt;&lt;b&gt;Exit&lt;/b&gt; to terminate VirtualBox without saving the results of the conversion to disk.&lt;/li&gt;</source>
        <translation type="obsolete">&lt;li&gt;&lt;b&gt;Выход&lt;/b&gt; для завершения VirtualBox без сохранения результатов преобразования на диск.&lt;/li&gt;</translation>
    </message>
    <message>
        <source>O&amp;verwrite</source>
        <comment>warnAboutAutoConvertedSettings message box</comment>
        <translation type="obsolete">&amp;Перезаписать</translation>
    </message>
    <message>
        <source>&lt;p&gt;A critical error has occurred while running the virtual machine and the machine execution has been stopped.&lt;/p&gt;&lt;p&gt;For help, please see the Community section on &lt;a href=http://www.virtualbox.org&gt;http://www.virtualbox.org&lt;/a&gt; or your support contract. Please provide the contents of the log file &lt;tt&gt;VBox.log&lt;/tt&gt; and the image file &lt;tt&gt;VBox.png&lt;/tt&gt;, which you can find in the &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/nobr&gt; directory, as well as a description of what you were doing when this error happened. Note that you can also access the above files by selecting &lt;b&gt;Show Log&lt;/b&gt; from the &lt;b&gt;Machine&lt;/b&gt; menu of the main VirtualBox window.&lt;/p&gt;&lt;p&gt;Press &lt;b&gt;OK&lt;/b&gt; if you want to power off the machine or press &lt;b&gt;Ignore&lt;/b&gt; if you want to leave it as is for debugging. Please note that debugging requires special knowledge and tools, so it is recommended to press &lt;b&gt;OK&lt;/b&gt; now.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Во время работы виртуальной машины произошла критическая ошибка. Выполнение виртуальной машины приостановлено.&lt;/p&gt;&lt;p&gt;Вы можете обратиться за помощью к разделу Community на веб-сайте &lt;a href=http://www.virtualbox.org&gt;http://www.virtualbox.org&lt;/a&gt;, либо к Вашему контракту на поддержку и сопровождение продукта. Пожалуйста, предоставьте содержимое журнала &lt;tt&gt;VBox.log&lt;/tt&gt; и изображение &lt;tt&gt;VBox.png&lt;/tt&gt;, которые находятся в папке &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/nobr&gt;, а также подробное описание того, что Вы делали, когда возникла эта ошибка. Получить доступ к вышеуказанным файлам можно также через пункт &lt;b&gt;Показать журнал&lt;/b&gt; в меню &lt;b&gt;Машина&lt;/b&gt; главного окна VirualBox.&lt;/p&gt;&lt;p&gt;Нажмите &lt;b&gt;ОК&lt;/b&gt;, если Вы хотите выключить виртуальную машину, либо нажмите &lt;b&gt;Игнорировать&lt;/b&gt;, если Вы хотите оставить ее в текущем состоянии для отладки. Обратите внимание, что отладка требует наличия специальных инструментов и навыков, поэтому рекомендуется просто выбрать &lt;b&gt;ОК&lt;/b&gt;.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>hard disk</source>
        <comment>failed to close ...</comment>
        <translation type="obsolete">жесткий диск</translation>
    </message>
    <message>
        <source>CD/DVD image</source>
        <comment>failed to close ...</comment>
        <translation type="obsolete">образ CD/DVD</translation>
    </message>
    <message>
        <source>floppy image</source>
        <comment>failed to close ...</comment>
        <translation type="obsolete">образ дискеты</translation>
    </message>
    <message>
        <source>A file named &lt;b&gt;%1&lt;/b&gt; already exists. Are you sure you want to replace it?&lt;br /&gt;&lt;br /&gt;The file already exists in &quot;%2&quot;. Replacing it will overwrite its contents.</source>
        <translation type="obsolete">Файл с именем &lt;b&gt;%1&lt;/b&gt; уже существует. Вы уверены, что хотите его заменить?&lt;br /&gt;&lt;br /&gt;Данный файл расположен в &quot;%2&quot;. Замена приведёт к перезаписи его содержимого.</translation>
    </message>
    <message>
        <source>The following files already exist:&lt;br /&gt;&lt;br /&gt;%1&lt;br /&gt;&lt;br /&gt;Are you sure you want to replace them? Replacing them will overwrite their contents.</source>
        <translation>Следующие файлы уже существуют:&lt;br /&gt;&lt;br /&gt;%1&lt;br /&gt;&lt;br /&gt;Вы уверены, что хотите их заменить? Замена приведёт к перезаписи их содержимого.</translation>
    </message>
    <message>
        <source>Failed to remove the file &lt;b&gt;%1&lt;/b&gt;.&lt;br /&gt;&lt;br /&gt;Please try to remove the file yourself and try again.</source>
        <translation type="obsolete">Не удалось удалить файл &lt;b&gt;%1&lt;/b&gt;.&lt;br /&gt;&lt;br /&gt;Пожалуйста удалите его самостоятельно и повторите попытку.</translation>
    </message>
    <message>
        <source>You are running a prerelease version of VirtualBox. This version is not suitable for production use.</source>
        <translation>Вы запустили предрелизную версию VirtualBox. Данная версия не предназначена для использования в качестве конечного продукта.</translation>
    </message>
    <message>
        <source>Could not access USB on the host system, because neither the USB file system (usbfs) nor the DBus and hal services are currently available. If you wish to use host USB devices inside guest systems, you must correct this and restart VirtualBox.</source>
        <translation type="obsolete">Не удаётся получить доступ к USB в основной операционной системе поскольку ни файловая система USB (usbfs), ни сервисы (DBus и hal) не доступны в данный момент. Если Вы хотите использовать USB устройства основной операционной системы в гостевой, Вам необходимо исправить данную проблему и перезапустить VirtualBox.</translation>
    </message>
    <message>
        <source>You are trying to shut down the guest with the ACPI power button. This is currently not possible because the guest does not support software shutdown.</source>
        <translation>Вы пытаетесь завершить работу гостевой операционной системы с использованием виртуальной кнопки питания ACPI. В данный момент это не возможно поскольку гостевая операционная система не использует подсистему ACPI.</translation>
    </message>
    <message>
        <source>&lt;p&gt;VT-x/AMD-V hardware acceleration has been enabled, but is not operational. Your 64-bit guest will fail to detect a 64-bit CPU and will not be able to boot.&lt;/p&gt;&lt;p&gt;Please ensure that you have enabled VT-x/AMD-V properly in the BIOS of your host computer.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Функции аппаратной виртуализации VT-x/AMD-V включены, но не функционируют. Ваша 64х-битная гостевая операционная система не сможет определить 64х-битный процессор и, таким образом, не сможет загрузиться.&lt;/p&gt;&lt;p&gt;Пожалуйста убедитесь в том, что функции аппаратной виртуализации VT-x/AMD-V корректно включены в BIOS Вашего компьютера.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Close VM</source>
        <translation>Закрыть ВМ</translation>
    </message>
    <message>
        <source>Continue</source>
        <translation>Продолжить</translation>
    </message>
    <message>
        <source>&lt;p&gt;Are you sure you wish to delete the selected snapshot and saved state?&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Вы уверены, что хотите сбросить выбранный снимок и состояние машины?&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Discard</source>
        <translation type="obsolete">Сбросить</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation>Отмена</translation>
    </message>
    <message>
        <source>&lt;p&gt;There are hard disks attached to ports of the additional controller. If you disable the additional controller, all these hard disks will be automatically detached.&lt;/p&gt;&lt;p&gt;Are you sure you want to disable the additional controller?&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Имеются жёсткие диски, подсоединённые к портам дополнительного контроллера. Если Вы отключите дополнительный контроллер, все эти жёсткие диски будут автоматически отсоединены.&lt;/p&gt;&lt;p&gt;Вы уверены, что хотите отключить дополнительный контроллер?&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;There are hard disks attached to ports of the additional controller. If you change the additional controller, all these hard disks will be automatically detached.&lt;/p&gt;&lt;p&gt;Are you sure you want to change the additional controller?&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Имеются жёсткие диски, подсоединённые к портам дополнительного контроллера. Если Вы измените дополнительный контроллер, все эти жёсткие диски будут автоматически отсоединены.&lt;/p&gt;&lt;p&gt;Вы уверены, что хотите изменить дополнительный контроллер?&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Change</source>
        <comment>hard disk</comment>
        <translation type="obsolete">Изменить</translation>
    </message>
    <message>
        <source>&lt;p&gt;Do you want to remove the selected host network interface &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;?&lt;/nobr&gt;&lt;/p&gt;&lt;p&gt;&lt;b&gt;Note:&lt;/b&gt; This interface may be in use by one or more network adapters of this or another VM. After it is removed, these adapters will no longer work until you correct their settings by either choosing a different interface name or a different adapter attachment type.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Вы уверены, что хотите удалить выбранный сетевой адаптер хоста &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;?&lt;/nobr&gt;&lt;/p&gt;&lt;p&gt;&lt;b&gt;Примечание:&lt;/b&gt; Этот адаптер может использоваться виртуальными сетевыми адаптерами нескольких ВМ. После его удаления такие виртуальные адаптеры не будут работать, пока Вы не исправите их настройки выбором другого сетевого адаптера хоста или изменением типа подключения виртуального адаптера.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Failed to create the host-only network interface.</source>
        <translation>Не удалось создать виртуальный сетевой адаптер основной операционной системы.</translation>
    </message>
    <message>
        <source>&lt;p&gt;Your existing VirtualBox settings files will be automatically converted from the old format to a new format required by the new version of VirtualBox.&lt;/p&gt;&lt;p&gt;Press &lt;b&gt;OK&lt;/b&gt; to start VirtualBox now or press &lt;b&gt;Exit&lt;/b&gt; if you want to terminate the VirtualBox application without any further actions.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Существующие файлы настроек VirtualBox будут автоматически сконвертированы из старого формата в новый, необходимый для новой версии VirtualBox.&lt;/p&gt;&lt;p&gt;Нажмите &lt;b&gt;ОК&lt;/b&gt; для запуска VirtualBox сейчас, либо &lt;b&gt;Выход&lt;/b&gt;, если желаете завершить работу VirtualBox без дальнейших действий.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Failed to open appliance.</source>
        <translation>Не удалось открыть конфигурацию.</translation>
    </message>
    <message>
        <source>Failed to open/interpret appliance &lt;b&gt;%1&lt;/b&gt;.</source>
        <translation>Не удалось прочитать конфигурацию &lt;b&gt;%1&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Failed to import appliance &lt;b&gt;%1&lt;/b&gt;.</source>
        <translation>Не удалось импортировать конфигурацию &lt;b&gt;%1&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Failed to create appliance.</source>
        <translation>Не удалось создать конфигурацию.</translation>
    </message>
    <message>
        <source>Failed to prepare the export of the appliance &lt;b&gt;%1&lt;/b&gt;.</source>
        <translation>Не удалось подготовить экспорт конфигурации &lt;b&gt;%1&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Failed to create an appliance.</source>
        <translation>Не удалось создать конфигурацию.</translation>
    </message>
    <message>
        <source>Failed to export appliance &lt;b&gt;%1&lt;/b&gt;.</source>
        <translation>Не удалось экспортировать конфигурацию &lt;b&gt;%1&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>&lt;p&gt;Deleting this host-only network will remove the host-only interface this network is based on. Do you want to remove the (host-only network) interface &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;?&lt;/nobr&gt;&lt;/p&gt;&lt;p&gt;&lt;b&gt;Note:&lt;/b&gt; this interface may be in use by one or more virtual network adapters belonging to one of your VMs. After it is removed, these adapters will no longer be usable until you correct their settings by either choosing a different interface name or a different adapter attachment type.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Удаление данной виртуальной сети хоста приведёт к удалению виртуального сетевого адаптера хоста, на котором основана данная сеть. Хотите ли Вы удалить адаптер &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;?&lt;/nobr&gt;&lt;/p&gt;&lt;p&gt;&lt;b&gt;Примечание:&lt;/b&gt; этот виртуальный сетевой адаптер хоста может использоваться в данный момент одним или более виртуальным сетевым адаптером гостя, принадлежащим одной из Ваших ВМ. После того как он будет удалён, эти гостевые адаптеры не будут функционировать до тех пор, пока Вы не скорректируете их настройки, выбрав другой виртуальный сетевой адаптер хоста или иной тип подключения к сети.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>A file named &lt;b&gt;%1&lt;/b&gt; already exists. Are you sure you want to replace it?&lt;br /&gt;&lt;br /&gt;Replacing it will overwrite its contents.</source>
        <translation>Файл с именем &lt;b&gt;%1&lt;/b&gt; уже существует. Вы уверены, что хотите его заменить?&lt;br /&gt;&lt;br /&gt;Замена приведёт к перезаписи его содержимого.</translation>
    </message>
    <message>
        <source>&lt;p&gt;VT-x/AMD-V hardware acceleration has been enabled, but is not operational. Certain guests (e.g. OS/2 and QNX) require this feature.&lt;/p&gt;&lt;p&gt;Please ensure that you have enabled VT-x/AMD-V properly in the BIOS of your host computer.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Функции аппаратной виртуализации VT-x/AMD-V включены, но не функционируют. Некоторым гостевым операционным системам (таким как OS/2 и QNX) эти функции необходимы&lt;/p&gt;&lt;p&gt;Пожалуйста убедитесь в том, что функции аппаратной виртуализации VT-x/AMD-V корректно включены в BIOS Вашего компьютера.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Invalid e-mail address or password specified.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Указан неверный адрес электронной почты либо неверный пароль.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Failed to register the VirtualBox product.&lt;/p&gt;&lt;p&gt;%1&lt;/p&gt;</source>
        <translation>&lt;p&gt;Не удалось зарегистрировать VirtualBox.&lt;/p&gt;&lt;p&gt;%1&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Failed to check files.</source>
        <translation>Не удалось проверить файлы.</translation>
    </message>
    <message>
        <source>Failed to remove file.</source>
        <translation>Не удалось удалить файлы.</translation>
    </message>
    <message>
        <source>You seem to have the USBFS filesystem mounted at /sys/bus/usb/drivers. We strongly recommend that you change this, as it is a severe mis-configuration of your system which could cause USB devices to fail in unexpected ways.</source>
        <translation>По всей видимости, Ваша файловая система USBFS монтирована в /sys/bus/usb/drivers. Мы настоятельно рекомендуем Вам исправить данную ошибочную конфигурацию, поскольку она может привести к неработоспособности USB устройств.</translation>
    </message>
    <message>
        <source>You are running an EXPERIMENTAL build of VirtualBox. This version is not suitable for production use.</source>
        <translation>Вы запустили ЭКСПЕРИМЕНТАЛЬНУЮ версию VirtualBox. Данная версия не предназначена для использования в качестве конечного продукта.</translation>
    </message>
    <message>
        <source>&lt;p&gt;Are you sure you want to restore snapshot &lt;b&gt;%1&lt;/b&gt;? This will cause you to lose your current machine state, which cannot be recovered.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Вы уверены, что хотите восстановить снимок &lt;b&gt;%1&lt;/b&gt;? Это приведёт к потере текущего состояния данной машины, которое не может быть восстановлено в дальнейшем.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Restore</source>
        <translation>Восстановить</translation>
    </message>
    <message>
        <source>&lt;p&gt;Deleting the snapshot will cause the state information saved in it to be lost, and disk data spread over several image files that VirtualBox has created together with the snapshot will be merged into one file. This can be a lengthy process, and the information in the snapshot cannot be recovered.&lt;/p&gt;&lt;/p&gt;Are you sure you want to delete the selected snapshot &lt;b&gt;%1&lt;/b&gt;?&lt;/p&gt;</source>
        <translation>&lt;p&gt;При удалении снимка информация о состоянии машины, хранящаяся в данном снимке, будет уничтожена, а данные, хранящиеся в файлах, созданных VirtualBox при создании снимка будут объединены в один файл. Данный процесс может занять некоторое время, а информация хранящаяся в снимке не может быть в последствии восстановлена.&lt;/p&gt;&lt;/p&gt;Вы уверены, что хотите удалить выбранный снимок &lt;b&gt;%1&lt;/b&gt;?&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Delete</source>
        <translation>Удалить</translation>
    </message>
    <message>
        <source>Failed to restore the snapshot &lt;b&gt;%1&lt;/b&gt; of the virtual machine &lt;b&gt;%2&lt;/b&gt;.</source>
        <translation>Не удалось восстановить снимок &lt;b&gt;%1&lt;/b&gt; виртуальной машины &lt;b&gt;%2&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Failed to delete the snapshot &lt;b&gt;%1&lt;/b&gt; of the virtual machine &lt;b&gt;%2&lt;/b&gt;.</source>
        <translation>Не удалось удалить снимок &lt;b&gt;%1&lt;/b&gt; виртуальной машины &lt;b&gt;%2&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>&lt;p&gt;There are no unused media available for the newly created attachment.&lt;/p&gt;&lt;p&gt;Press the &lt;b&gt;Create&lt;/b&gt; button to start the &lt;i&gt;New Virtual Disk&lt;/i&gt; wizard and create a new medium, or press the &lt;b&gt;Select&lt;/b&gt; if you wish to open the &lt;i&gt;Virtual Media Manager&lt;/i&gt;.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Нет свободных виртуальных устройств доступных для созданного подключения.&lt;/p&gt;&lt;p&gt;Нажмите кнопку &lt;b&gt;Создать&lt;/b&gt; для запуска мастера &lt;i&gt;нового виртуального диска&lt;/i&gt; и создайте необходимое устройство, или нажмите кнопку &lt;b&gt;Выбрать&lt;/b&gt; если желаете открыть &lt;i&gt;менеджер виртуальных носителей&lt;/i&gt;.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&amp;Create</source>
        <comment>medium</comment>
        <translation type="obsolete">&amp;Создать</translation>
    </message>
    <message>
        <source>&amp;Select</source>
        <comment>medium</comment>
        <translation type="obsolete">&amp;Выбрать</translation>
    </message>
    <message>
        <source>&lt;p&gt;There are no unused media available for the newly created attachment.&lt;/p&gt;&lt;p&gt;Press the &lt;b&gt;Select&lt;/b&gt; if you wish to open the &lt;i&gt;Virtual Media Manager&lt;/i&gt;.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Нет свободных виртуальных устройств доступных для созданного подключения.&lt;/p&gt;&lt;p&gt;Нажмите кнопку &lt;b&gt;Выбрать&lt;/b&gt; если желаете открыть &lt;i&gt;менеджер виртуальных носителей&lt;/i&gt;.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Failed to attach the %1 to slot &lt;i&gt;%2&lt;/i&gt; of the machine &lt;b&gt;%3&lt;/b&gt;.</source>
        <translation type="obsolete">Не удалось подсоединить %1 к слоту &lt;i&gt;%2&lt;/i&gt; виртуальной машины &lt;b&gt;%3&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Failed to detach the %1 from slot &lt;i&gt;%2&lt;/i&gt; of the machine &lt;b&gt;%3&lt;/b&gt;.</source>
        <translation type="obsolete">Не удалось отсоединить %1 от слота &lt;i&gt;%2&lt;/i&gt; виртуальной машины &lt;b&gt;%3&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Unable to mount the %1 &lt;nobr&gt;&lt;b&gt;%2&lt;/b&gt;&lt;/nobr&gt; on the machine &lt;b&gt;%3&lt;/b&gt;.</source>
        <translation>Не удалось подключить %1 &lt;nobr&gt;&lt;b&gt;%2&lt;/b&gt;&lt;/nobr&gt; к машине &lt;b&gt;%3&lt;/b&gt;.</translation>
    </message>
    <message>
        <source> Would you like to force mounting of this medium?</source>
        <translation> Не желаете ли произвести силовое подключение данного устройства?</translation>
    </message>
    <message>
        <source>Unable to unmount the %1 &lt;nobr&gt;&lt;b&gt;%2&lt;/b&gt;&lt;/nobr&gt; from the machine &lt;b&gt;%3&lt;/b&gt;.</source>
        <translation>Не удалось отключить %1 &lt;nobr&gt;&lt;b&gt;%2&lt;/b&gt;&lt;/nobr&gt; от машины &lt;b&gt;%3&lt;/b&gt;.</translation>
    </message>
    <message>
        <source> Would you like to force unmounting of this medium?</source>
        <translation> Не желаете ли произвести силовое отключение данного устройства?</translation>
    </message>
    <message>
        <source>Force Unmount</source>
        <translation>Желаю</translation>
    </message>
    <message>
        <source>Failed to eject the disk from the virtual drive. The drive may be locked by the guest operating system. Please check this and try again.</source>
        <translation type="obsolete">Не удалось извлечь диск из виртуального привода. Возможно данный привод используется гостевой операционной системой. Пожалуйста проверьте данный факт и попробуйте снова.</translation>
    </message>
    <message>
        <source>&lt;p&gt;Could not insert the VirtualBox Guest Additions installer CD image into the virtual machine &lt;b&gt;%1&lt;/b&gt;, as the machine has no CD/DVD-ROM drives. Please add a drive using the storage page of the virtual machine settings dialog.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Не удалось подключить установочный пакет гостевых дополнений VirtualBox &lt;b&gt;%1&lt;/b&gt;, поскольку данная машина не имеет привода оптических дисков. Пожалуйста добавьте привод, используя страницу носителей информации диалога настройки виртуальной машины.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>E&amp;xit</source>
        <comment>warnAboutSettingsAutoConversion message box</comment>
        <translation>&amp;Выход</translation>
    </message>
    <message>
        <source>&lt;p&gt;The following VirtualBox settings files will be automatically converted from the old format to a new format required by the new version of VirtualBox.&lt;/p&gt;&lt;p&gt;Press &lt;b&gt;OK&lt;/b&gt; to start VirtualBox now or press &lt;b&gt;Exit&lt;/b&gt; if you want to terminate the VirtualBox application without any further actions.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Текущие файлы настроек VirtualBox будут автоматически сконвертированы из старого формата в новый, необходимый новой версии VirtualBox.&lt;/p&gt;&lt;p&gt;Нажмите &lt;b&gt;Согласен&lt;/b&gt; для запуска VirtualBox или &lt;b&gt;Выход&lt;/b&gt; если желаете завершить работу VirtualBox, не выполняя конвертации.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>hard disk</source>
        <comment>failed to mount ...</comment>
        <translation type="unfinished">жёсткий диск</translation>
    </message>
    <message>
        <source>CD/DVD</source>
        <comment>failed to mount ... host-drive</comment>
        <translation type="unfinished">CD/DVD</translation>
    </message>
    <message>
        <source>CD/DVD image</source>
        <comment>failed to mount ...</comment>
        <translation type="unfinished">образ оптического диска</translation>
    </message>
    <message>
        <source>floppy</source>
        <comment>failed to mount ... host-drive</comment>
        <translation type="unfinished">Floppy</translation>
    </message>
    <message>
        <source>floppy image</source>
        <comment>failed to mount ...</comment>
        <translation type="unfinished">образ гибкого диска</translation>
    </message>
    <message>
        <source>hard disk</source>
        <comment>failed to attach ...</comment>
        <translation type="obsolete">жёсткий диск</translation>
    </message>
    <message>
        <source>CD/DVD device</source>
        <comment>failed to attach ...</comment>
        <translation type="obsolete">привод оптических дисков</translation>
    </message>
    <message>
        <source>floppy device</source>
        <comment>failed to close ...</comment>
        <translation type="obsolete">привод гибких дисков</translation>
    </message>
    <message>
        <source>&lt;p&gt;Are you sure you want to delete the CD/DVD-ROM device?&lt;/p&gt;&lt;p&gt;You will not be able to mount any CDs or ISO images or install the Guest Additions without it!&lt;/p&gt;</source>
        <translation>&lt;p&gt;Вы уверены, что хотите удалить виртуальный привод оптических дисков?&lt;/p&gt;&lt;p&gt;Без такого устройства у Вас не будет возможности подключать привод оптических дисков основного компьютера или образ оптического диска к виртуальной машине. Кроме того, Вы не сможете установить дополнения гостевой операционной системы!&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&amp;Remove</source>
        <comment>medium</comment>
        <translation>&amp;Удалить</translation>
    </message>
    <message>
        <source>&lt;p&gt;VT-x/AMD-V hardware acceleration is not available on your system. Your 64-bit guest will fail to detect a 64-bit CPU and will not be able to boot.</source>
        <translation>&lt;p&gt;Аппаратное ускорение (VT-x/AMD-V) не доступно в Вашей системе. Ваша 64х-битная гостевая операционная система не сможет определить 64х-битный процессор и, таким образом, не сможет загрузиться.</translation>
    </message>
    <message>
        <source>&lt;p&gt;VT-x/AMD-V hardware acceleration is not available on your system. Certain guests (e.g. OS/2 and QNX) require this feature and will fail to boot without it.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Аппаратное ускорение (VT-x/AMD-V) не доступно в Вашей системе. Некоторым операционным системам (таким как OS/2 и QNX) данный функционал необходим, они не смогут загрузиться без него.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Deleting the snapshot %1 will temporarily need more disk space. In the worst case the size of image %2 will grow by %3, however on this filesystem there is only %4 free.&lt;/p&gt;&lt;p&gt;Running out of disk space during the merge operation can result in corruption of the image and the VM configuration, i.e. loss of the VM and its data.&lt;/p&gt;&lt;p&gt;You may continue with deleting the snapshot at your own risk.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Для удаление снимка %1 временно понадобится некоторое количество свободного дискового пространства. В самом худшем случае размер образа %2 увеличится на %3, однако в данной системе доступно лишь %4.&lt;/p&gt;&lt;p&gt;В случае нехватки свободного места в процессе операции объединения образов, может произойти критическая ошибка, что приведёт к повреждению образа и конфигурации виртуальной машины, иными словами - к потере данной машины и её данных.&lt;/p&gt;&lt;p&gt;Однако, Вы все же можете попытаться удалить данный снимок на свой страх и риск.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Could not change the guest screen to this host screen due to insufficient guest video memory.&lt;/p&gt;&lt;p&gt;You should configure the virtual machine to have at least &lt;b&gt;%1&lt;/b&gt; of video memory.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Не удалось сопоставить монитор гостевой машины с данным монитором хоста из-за нехватки видео-памяти.&lt;/p&gt;&lt;p&gt;Вам следует настроить виртуальную машину таким образом, что бы она имела как минимум &lt;b&gt;%1&lt;/b&gt; видео-памяти.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Could not change the guest screen to this host screen due to insufficient guest video memory.&lt;/p&gt;&lt;p&gt;You should configure the virtual machine to have at least &lt;b&gt;%1&lt;/b&gt; of video memory.&lt;/p&gt;&lt;p&gt;Press &lt;b&gt;Ignore&lt;/b&gt; to switch the screen anyway or press &lt;b&gt;Cancel&lt;/b&gt; to cancel the operation.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Не удалось сопоставить монитор гостевой машины с данным монитором хоста из-за нехватки видео-памяти.&lt;/p&gt;&lt;p&gt;Вам следует настроить виртуальную машину таким образом, что бы она имела как минимум &lt;b&gt;%1&lt;/b&gt; видео-памяти.&lt;/p&gt;&lt;p&gt;Нажмите кнопку &lt;b&gt;Игнорировать&lt;/b&gt; чтобы попытаться это сделать в любом случае или &lt;b&gt;Отмена&lt;/b&gt; чтобы прервать данную операцию.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Can not switch the guest display to fullscreen mode. You have more virtual screens configured than physical screens are attached to your host.&lt;/p&gt;&lt;p&gt;Please either lower the virtual screens in your VM configuration or attach additional screens to your host.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Не удалось переключить гостевую операционную систему в режим полного экрана. Данная машина настроена на поддержку большего количества виртуальных мониторов, чем реально имеется на Вашем хосте.&lt;/p&gt;&lt;p&gt;Пожалуйста уменьшите количество виртуальных мониторов в настройках Вашей машины, либо подключите дополнительные мониторы к Вашему хосту.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Can not switch the guest display to seamless mode. You have more virtual screens configured than physical screens are attached to your host.&lt;/p&gt;&lt;p&gt;Please either lower the virtual screens in your VM configuration or attach additional screens to your host.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Не удалось переключить гостевую операционную систему в режим интеграции экрана. Данная машина настроена на поддержку большего количества виртуальных мониторов, чем реально имеется на Вашем хосте.&lt;/p&gt;&lt;p&gt;Пожалуйста уменьшите количество виртуальных мониторов в настройках Вашей машины, либо подключите дополнительные мониторы к Вашему хосту.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Could not find the VirtualBox User Manual &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;.&lt;/nobr&gt;&lt;/p&gt;&lt;p&gt;Do you wish to download this file from the Internet?&lt;/p&gt;</source>
        <translation>&lt;p&gt;Не удалось обнаружить Руководство Пользователя VirtualBox &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;.&lt;/nobr&gt;&lt;/p&gt;&lt;p&gt;Не желаете ли загрузить данный документ с сайта в сети Интернет?&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Are you sure you want to download the VirtualBox User Manual from &lt;nobr&gt;&lt;a href=&quot;%1&quot;&gt;%2&lt;/a&gt;&lt;/nobr&gt; (size %3 bytes)?&lt;/p&gt;</source>
        <translation>&lt;p&gt;Вы уверены, что хотите загрузить Руководство Пользователя VirtualBox, находящееся по адресу &lt;nobr&gt;&lt;a href=&quot;%1&quot;&gt;%2&lt;/a&gt;&lt;/nobr&gt; (размер %3 б)?&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Failed to download the VirtualBox User Manual from &lt;nobr&gt;&lt;a href=&quot;%1&quot;&gt;%2&lt;/a&gt;.&lt;/nobr&gt;&lt;/p&gt;&lt;p&gt;%3&lt;/p&gt;</source>
        <translation>&lt;p&gt;Не удалось загрузить Руководство Пользователя VirtualBox, находящееся по адресу &lt;nobr&gt;&lt;a href=&quot;%1&quot;&gt;%2&lt;/a&gt;.&lt;/nobr&gt;&lt;/p&gt;&lt;p&gt;%3&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;The VirtualBox User Manual has been successfully downloaded from &lt;nobr&gt;&lt;a href=&quot;%1&quot;&gt;%2&lt;/a&gt;&lt;/nobr&gt; and saved locally as &lt;nobr&gt;&lt;b&gt;%3&lt;/b&gt;.&lt;/nobr&gt;&lt;/p&gt;</source>
        <translation>&lt;p&gt;Руководство Пользователя VirtualBox было успешно загружено с сетевого адреса &lt;nobr&gt;&lt;a href=&quot;%1&quot;&gt;%2&lt;/a&gt;&lt;/nobr&gt; и сохранено локально по адресу &lt;nobr&gt;&lt;b&gt;%3&lt;/b&gt;.&lt;/nobr&gt;&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;The VirtualBox User Manual has been successfully downloaded from &lt;nobr&gt;&lt;a href=&quot;%1&quot;&gt;%2&lt;/a&gt;&lt;/nobr&gt; but can&apos;t be saved locally as &lt;nobr&gt;&lt;b&gt;%3&lt;/b&gt;.&lt;/nobr&gt;&lt;/p&gt;&lt;p&gt;Please choose another location for that file.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Руководство Пользователя VirtualBox было успешно загружено с сетевого адреса &lt;nobr&gt;&lt;a href=&quot;%1&quot;&gt;%2&lt;/a&gt;&lt;/nobr&gt;, но не может быть сохранено локально по адресу &lt;nobr&gt;&lt;b&gt;%3&lt;/b&gt;.&lt;/nobr&gt;&lt;/p&gt;&lt;p&gt;Пожалуйста укажите иное местоположение для данного файла.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Failed to open virtual machine located in %1.</source>
        <translation>Не удалось открыть виртуальную машину, расположенную в %1.</translation>
    </message>
    <message>
        <source>Failed to add virtual machine &lt;b&gt;%1&lt;/b&gt; located in &lt;i&gt;%2&lt;/i&gt; because its already present.</source>
        <translation>Не удалось добавить виртуальную машину &lt;b&gt;%1&lt;/b&gt;, расположенную в &lt;i&gt;%2&lt;/i&gt; поскольку она была добавлена ранее.</translation>
    </message>
    <message>
        <source>&lt;p&gt;You are about to remove the virtual machine &lt;b&gt;%1&lt;/b&gt; from the machine list.&lt;/p&gt;&lt;p&gt;Would you like to delete the files containing the virtual machine from your hard disk as well?&lt;/p&gt;</source>
        <translation>&lt;p&gt;Вы собираетесь убрать витуальную машину &lt;b&gt;%1&lt;/b&gt; из списка.&lt;/p&gt;&lt;p&gt;Не желаете ли удалить также и файлы конфигурации данной машины с Вашего жёсткого диска?&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;You are about to remove the virtual machine &lt;b&gt;%1&lt;/b&gt; from the machine list.&lt;/p&gt;&lt;p&gt;Would you like to delete the files containing the virtual machine from your hard disk as well? Doing this will also remove the files containing the machine&apos;s virtual hard disks if they are not in use by another machine.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Вы собираетесь убрать виртуальную машину &lt;b&gt;%1&lt;/b&gt; из списка.&lt;/p&gt;&lt;p&gt;Не желаете ли удалить также и файлы конфигурации данной машины с Вашего жёсткого диска? Учтите, что это, в том числе, подразумевает удаление файлов, содержащих виртуальные жёсткие диски данной машины в случае, если они не используются другими машинами.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Delete all files</source>
        <translation>Удалить все файлы</translation>
    </message>
    <message>
        <source>Remove only</source>
        <translation>Убрать из списка</translation>
    </message>
    <message>
        <source>You are about to remove the inaccessible virtual machine &lt;b&gt;%1&lt;/b&gt; from the machine list. Do you wish to proceed?</source>
        <translation>Вы собираетесь убрать из списка недоступную в данный момент машину &lt;b&gt;%1&lt;/b&gt;. Желаете продолжить?</translation>
    </message>
    <message>
        <source>Remove</source>
        <translation>Убрать</translation>
    </message>
    <message>
        <source>&lt;p&gt;You are about to add a virtual hard disk to controller &lt;b&gt;%1&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Would you like to create a new, empty file to hold the disk contents or select an existing one?&lt;/p&gt;</source>
        <translation>&lt;p&gt;Вы собираетесь добавить виртуальный жёсткий диск к контроллеру &lt;b&gt;%1&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Желаете создать новый пустой файл для хранения содержимого диска или выбрать существующий?&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Create &amp;new disk</source>
        <comment>add attachment routine</comment>
        <translation>&amp;Создать новый диск</translation>
    </message>
    <message>
        <source>&amp;Choose existing disk</source>
        <comment>add attachment routine</comment>
        <translation>&amp;Выбрать существующий диск</translation>
    </message>
    <message>
        <source>&lt;p&gt;You are about to add a new CD/DVD drive to controller &lt;b&gt;%1&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Would you like to choose a virtual CD/DVD disk to put in the drive or to leave it empty for now?&lt;/p&gt;</source>
        <translation>&lt;p&gt;Вы собираетесь добавить новый привод оптических дисков к контроллеру &lt;b&gt;%1&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Желаете выбрать образ оптического диска и поместить его в данный привод или оставить привод пустым?&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&amp;Choose disk</source>
        <comment>add attachment routine</comment>
        <translation>&amp;Выбрать образ</translation>
    </message>
    <message>
        <source>Leave &amp;empty</source>
        <comment>add attachment routine</comment>
        <translation>Оставить &amp;пустым</translation>
    </message>
    <message>
        <source>&lt;p&gt;You are about to add a new floppy drive to controller &lt;b&gt;%1&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Would you like to choose a virtual floppy disk to put in the drive or to leave it empty for now?&lt;/p&gt;</source>
        <translation>&lt;p&gt;Вы собираетесь добавить новый привод гибких дисков к контроллеру &lt;b&gt;%1&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Желаете выбрать образ гибкого диска и поместить его в данный привод или оставить привод пустым?&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Failed to detach the hard disk (&lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/nobr&gt;) from the slot &lt;i&gt;%2&lt;/i&gt; of the machine &lt;b&gt;%3&lt;/b&gt;.</source>
        <translation>Не удалось отсоединить жёсткий диск &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/nobr&gt; от слота &lt;i&gt;%2&lt;/i&gt; машины &lt;b&gt;%3&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Failed to detach the CD/DVD device (&lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/nobr&gt;) from the slot &lt;i&gt;%2&lt;/i&gt; of the machine &lt;b&gt;%3&lt;/b&gt;.</source>
        <translation>Не удалось отсоединить привод оптических дисков &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/nobr&gt; от слота &lt;i&gt;%2&lt;/i&gt; машины &lt;b&gt;%3&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Failed to detach the floppy device (&lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/nobr&gt;) from the slot &lt;i&gt;%2&lt;/i&gt; of the machine &lt;b&gt;%3&lt;/b&gt;.</source>
        <translation>Не удалось отсоединить привод гибких дисков &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/nobr&gt; от слота &lt;i&gt;%2&lt;/i&gt; машины &lt;b&gt;%3&lt;/b&gt;.</translation>
    </message>
    <message numerus="yes">
        <source>&lt;p&gt;The virtual machine(s) &lt;b&gt;%1&lt;/b&gt; are currently in a saved state.&lt;/p&gt;&lt;p&gt;If you continue the runtime state of the exported machine(s) will be discarded. Note that the existing machine(s) are not changed.&lt;/p&gt;</source>
        <translation>
            <numerusform>&lt;p&gt;Виртуальная машина &lt;b&gt;%1&lt;/b&gt; в данный момент находится в сохранённом состоянии.&lt;/p&gt;&lt;p&gt;Если Вы продолжите, рабочее состояние экспортируемой машины будет сброшено. Учтите, что существующие машины не будут изменены.&lt;/p&gt;</numerusform>
            <numerusform>&lt;p&gt;Виртуальные машины &lt;b&gt;%1&lt;/b&gt; в данный момент находятся в сохранённом состоянии.&lt;/p&gt;&lt;p&gt;Если Вы продолжите, рабочее состояние экспортируемых машины будет сброшено. Учтите, что существующие машины не будут изменены.&lt;/p&gt;</numerusform>
            <numerusform>&lt;p&gt;Виртуальные машины &lt;b&gt;%1&lt;/b&gt; в данный момент находятся в сохранённом состоянии.&lt;/p&gt;&lt;p&gt;Если Вы продолжите, рабочее состояние экспортируемых машины будет сброшено. Учтите, что существующие машины не будут изменены.&lt;/p&gt;</numerusform>
        </translation>
    </message>
    <message>
        <source>Failed to update Guest Additions. The Guest Additions installation image will be mounted to provide a manual installation.</source>
        <translation>Не удалось выполнить автоматическое обновление дополнений гостевой ОС. Образ дополнений гостевой ОС будет помещён в виртуальный привод оптических дисков виртуальной машины, что позволит выполнить Вам обновление вручную.</translation>
    </message>
    <message>
        <source>Failed to install the Extension Pack &lt;b&gt;%1&lt;/b&gt;.</source>
        <translation>Не удалось установить плагин &lt;b&gt;%1&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Failed to uninstall the Extension Pack &lt;b&gt;%1&lt;/b&gt;.</source>
        <translation>Не удалось удалить плагин &lt;b&gt;%1&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>&amp;Remove</source>
        <translation>&amp;Удалить</translation>
    </message>
    <message>
        <source>The current port forwarding rules are not valid. None of the host or guest port values may be set to zero.</source>
        <translation>Текущие правила проброса портов неверны. Значения хостовых и гостевых портов не могут быть равны нулю.</translation>
    </message>
    <message>
        <source>&lt;p&gt;There are unsaved changes in the port forwarding configuration.&lt;/p&gt;&lt;p&gt;If you proceed your changes will be discarded.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Настройки проброса портов не были сохранены.&lt;/p&gt;&lt;p&gt;Если Вы продолжите, эти настройки будут утеряны.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Sorry, some generic error happens.</source>
        <translation>Сожалеем, произошло что-то в корне невероятное...</translation>
    </message>
    <message>
        <source>Failed to attach the hard disk (&lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/nobr&gt;) to the slot &lt;i&gt;%2&lt;/i&gt; of the machine &lt;b&gt;%3&lt;/b&gt;.</source>
        <translation>Не удалось подсоединить жёсткий диск &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/nobr&gt; к слоту  &lt;i&gt;%2&lt;/i&gt; машины &lt;b&gt;%3&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Failed to attach the CD/DVD device (&lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/nobr&gt;) to the slot &lt;i&gt;%2&lt;/i&gt; of the machine &lt;b&gt;%3&lt;/b&gt;.</source>
        <translation>Не удалось подсоединить привод оптических дисков &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/nobr&gt; к слоту  &lt;i&gt;%2&lt;/i&gt; машины &lt;b&gt;%3&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>Failed to attach the floppy device (&lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/nobr&gt;) to the slot &lt;i&gt;%2&lt;/i&gt; of the machine &lt;b&gt;%3&lt;/b&gt;.</source>
        <translation>Не удалось подсоединить привод гибких дисков &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/nobr&gt; к слоту  &lt;i&gt;%2&lt;/i&gt; машины &lt;b&gt;%3&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>&lt;p&gt;Note that the storage unit of this medium will not be deleted and that it will be possible to use it later again.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Имейте в виду, что файл с данными этого носителя не будет удален и в дальнейшем может быть снова использован.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;The VirtualBox Guest Additions do not appear to be available on this virtual machine, and shared folders cannot be used without them. To use shared folders inside the virtual machine, please install the Guest Additions if they are not installed, or re-install them if they are not working correctly, by selecting &lt;b&gt;Install Guest Additions&lt;/b&gt; from the &lt;b&gt;Devices&lt;/b&gt; menu. If they are installed but the machine is not yet fully started then shared folders will be available once it is.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Не похоже, что дополнения гостевой ОС доступны данной виртуальной машине. Без них общие папки не могут быть использованы. Что бы использовать общие папки внутри виртуальной машины, пожалуйста, установите дополнения гостевой ОС если они не установлены, или переустановите их если они работают неверно, выбрав &lt;b&gt;Установить Дополнения гостевой ОС&lt;/b&gt; меню &lt;b&gt;Устройства&lt;/b&gt;. Если дополнения установлены но машина ещё не полностью загружена, они будут доступны на момент полной загрузки машины.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;The virtual machine window will be now switched to &lt;b&gt;fullscreen&lt;/b&gt; mode. You can go back to windowed mode at any time by pressing &lt;b&gt;%1&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Note that the &lt;i&gt;Host&lt;/i&gt; key is currently defined as &lt;b&gt;%2&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Note that the main menu bar is hidden in fullscreen mode. You can access it by pressing &lt;b&gt;Host+Home&lt;/b&gt;.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Сейчас окно виртуальной машины будет переключено в &lt;b&gt;полноэкранный&lt;/b&gt; режим. Вы можете вернуться в оконный режим в любое время, нажав &lt;b&gt;%1&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Обратите внимание, что в данный момент в качестве &lt;i&gt;хост-клавиши&lt;/i&gt; используется &lt;b&gt;%2&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Имейте в виду, что в полноэкранном режиме основное меню окна скрыто. Вы можете получить к нему доступ, нажав &lt;b&gt;Host+Home&lt;/b&gt;.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;The virtual machine window will be now switched to &lt;b&gt;Seamless&lt;/b&gt; mode. You can go back to windowed mode at any time by pressing &lt;b&gt;%1&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Note that the &lt;i&gt;Host&lt;/i&gt; key is currently defined as &lt;b&gt;%2&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Note that the main menu bar is hidden in seamless mode. You can access it by pressing &lt;b&gt;Host+Home&lt;/b&gt;.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Сейчас окно виртуальной машины будет переключено в режим &lt;b&gt;интеграции дисплея&lt;/b&gt;. Вы можете вернуться в оконный режим в любое время, нажав &lt;b&gt;%1&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Обратите внимание, что в данный момент в качестве &lt;i&gt;хост-клавиши&lt;/i&gt; используется &lt;b&gt;%2&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Имейте в виду, что в режиме интеграции дисплея основное меню окна скрыто. Вы можете получить к нему доступ, нажав &lt;b&gt;Host+Home&lt;/b&gt;.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;The virtual machine window will be now switched to &lt;b&gt;Scale&lt;/b&gt; mode. You can go back to windowed mode at any time by pressing &lt;b&gt;%1&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Note that the &lt;i&gt;Host&lt;/i&gt; key is currently defined as &lt;b&gt;%2&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Note that the main menu bar is hidden in scale mode. You can access it by pressing &lt;b&gt;Host+Home&lt;/b&gt;.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Сейчас окно виртуальной машины будет переключено в режим &lt;b&gt;масштабирования&lt;/b&gt;. Вы можете вернуться в оконный режим в любое время, нажав &lt;b&gt;%1&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Обратите внимание, что в данный момент в качестве &lt;i&gt;хост-клавиши&lt;/i&gt; используется &lt;b&gt;%2&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Имейте в виду, что в режиме масштабирования основное меню окна скрыто. Вы можете получить к нему доступ, нажав &lt;b&gt;Host+Home&lt;/b&gt;.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Switch</source>
        <comment>scale</comment>
        <translation>Переключить</translation>
    </message>
    <message>
        <source>Failed to open the Extension Pack &lt;b&gt;%1&lt;/b&gt;.</source>
        <translation>Не удалось открыть плагин &lt;b&gt;%1&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>&lt;p&gt;You are about to install a VirtualBox extension pack. Extension packs complement the functionality of VirtualBox and can contain system level software that could be potentially harmful to your system. Please review the description below and only proceed if you have obtained the extension pack from a trusted source.&lt;/p&gt;&lt;p&gt;&lt;table cellpadding=0 cellspacing=0&gt;&lt;tr&gt;&lt;td&gt;&lt;b&gt;Name:&amp;nbsp;&amp;nbsp;&lt;/b&gt;&lt;/td&gt;&lt;td&gt;%1&lt;/td&gt;&lt;/tr&gt;&lt;tr&gt;&lt;td&gt;&lt;b&gt;Version:&amp;nbsp;&amp;nbsp;&lt;/b&gt;&lt;/td&gt;&lt;td&gt;%2&lt;/td&gt;&lt;/tr&gt;&lt;tr&gt;&lt;td&gt;&lt;b&gt;Description:&amp;nbsp;&amp;nbsp;&lt;/b&gt;&lt;/td&gt;&lt;td&gt;%3&lt;/td&gt;&lt;/tr&gt;&lt;/table&gt;&lt;/p&gt;</source>
        <translation>&lt;p&gt;Вы собираетесь установить плагин VirtualBox. Плагины дополняют функциональность VirtualBox и могут представлять собой системные программы потенциально опасные для Вашей системы. Пожалуйста ознакомьтесь с описанием данного плагина и продолжайте лишь в том случае, если Вы получили плагин из достоверного источника.&lt;/p&gt;&lt;p&gt;&lt;table cellpadding=0 cellspacing=0&gt;&lt;tr&gt;&lt;td&gt;&lt;b&gt;Имя:&amp;nbsp;&amp;nbsp;&lt;/b&gt;&lt;/td&gt;&lt;td&gt;%1&lt;/td&gt;&lt;/tr&gt;&lt;tr&gt;&lt;td&gt;&lt;b&gt;Версия:&amp;nbsp;&amp;nbsp;&lt;/b&gt;&lt;/td&gt;&lt;td&gt;%2&lt;/td&gt;&lt;/tr&gt;&lt;tr&gt;&lt;td&gt;&lt;b&gt;Описание:&amp;nbsp;&amp;nbsp;&lt;/b&gt;&lt;/td&gt;&lt;td&gt;%3&lt;/td&gt;&lt;/tr&gt;&lt;/table&gt;&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&amp;Install</source>
        <translation>&amp;Установить</translation>
    </message>
    <message>
        <source>Extension packs complement the functionality of VirtualBox and can contain system level software that could be potentially harmful to your system. Please review the description below and only proceed if you have obtained the extension pack from a trusted source.</source>
        <translation>Плагины дополняют функциональность VirtualBox и могут представлять собой системные программы потенциально опасные для Вашей системы. Пожалуйста ознакомьтесь с описанием данного плагина и продолжайте лишь в том случае, если Вы получили плагин из достоверного источника.</translation>
    </message>
    <message>
        <source>&lt;p&gt;An older version of the extension pack is already installed, would you like to upgrade? &lt;p&gt;%1&lt;/p&gt;&lt;p&gt;&lt;table cellpadding=0 cellspacing=0&gt;&lt;tr&gt;&lt;td&gt;&lt;b&gt;Name:&amp;nbsp;&amp;nbsp;&lt;/b&gt;&lt;/td&gt;&lt;td&gt;%2&lt;/td&gt;&lt;/tr&gt;&lt;tr&gt;&lt;td&gt;&lt;b&gt;New Version:&amp;nbsp;&amp;nbsp;&lt;/b&gt;&lt;/td&gt;&lt;td&gt;%3&lt;/td&gt;&lt;/tr&gt;&lt;tr&gt;&lt;td&gt;&lt;b&gt;Current Version:&amp;nbsp;&amp;nbsp;&lt;/b&gt;&lt;/td&gt;&lt;td&gt;%4&lt;/td&gt;&lt;/tr&gt;&lt;tr&gt;&lt;td&gt;&lt;b&gt;Description:&amp;nbsp;&amp;nbsp;&lt;/b&gt;&lt;/td&gt;&lt;td&gt;%5&lt;/td&gt;&lt;/tr&gt;&lt;/table&gt;&lt;/p&gt;</source>
        <translation>&lt;p&gt;У Вас установлена более старая версия плагина. Не желаете ли её обновить?&lt;p&gt;%1&lt;/p&gt;&lt;p&gt;&lt;table cellpadding=0 cellspacing=0&gt;&lt;tr&gt;&lt;td&gt;&lt;b&gt;Имя:&amp;nbsp;&amp;nbsp;&lt;/b&gt;&lt;/td&gt;&lt;td&gt;%2&lt;/td&gt;&lt;/tr&gt;&lt;tr&gt;&lt;td&gt;&lt;b&gt;Новая версия:&amp;nbsp;&amp;nbsp;&lt;/b&gt;&lt;/td&gt;&lt;td&gt;%3&lt;/td&gt;&lt;/tr&gt;&lt;tr&gt;&lt;td&gt;&lt;b&gt;Текущая версия:&amp;nbsp;&amp;nbsp;&lt;/b&gt;&lt;/td&gt;&lt;td&gt;%4&lt;/td&gt;&lt;/tr&gt;&lt;tr&gt;&lt;td&gt;&lt;b&gt;Описание:&amp;nbsp;&amp;nbsp;&lt;/b&gt;&lt;/td&gt;&lt;td&gt;%5&lt;/td&gt;&lt;/tr&gt;&lt;/table&gt;&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&amp;Upgrade</source>
        <translation>&amp;Обновить</translation>
    </message>
    <message>
        <source>&lt;p&gt;An newer version of the extension pack is already installed, would you like to downgrade? &lt;p&gt;%1&lt;/p&gt;&lt;p&gt;&lt;table cellpadding=0 cellspacing=0&gt;&lt;tr&gt;&lt;td&gt;&lt;b&gt;Name:&amp;nbsp;&amp;nbsp;&lt;/b&gt;&lt;/td&gt;&lt;td&gt;%2&lt;/td&gt;&lt;/tr&gt;&lt;tr&gt;&lt;td&gt;&lt;b&gt;New Version:&amp;nbsp;&amp;nbsp;&lt;/b&gt;&lt;/td&gt;&lt;td&gt;%3&lt;/td&gt;&lt;/tr&gt;&lt;tr&gt;&lt;td&gt;&lt;b&gt;Current Version:&amp;nbsp;&amp;nbsp;&lt;/b&gt;&lt;/td&gt;&lt;td&gt;%4&lt;/td&gt;&lt;/tr&gt;&lt;tr&gt;&lt;td&gt;&lt;b&gt;Description:&amp;nbsp;&amp;nbsp;&lt;/b&gt;&lt;/td&gt;&lt;td&gt;%5&lt;/td&gt;&lt;/tr&gt;&lt;/table&gt;&lt;/p&gt;</source>
        <translation>&lt;p&gt;У Вас установлена более новая версия плагина. Желаете ли её откатить?&lt;p&gt;%1&lt;/p&gt;&lt;p&gt;&lt;table cellpadding=0 cellspacing=0&gt;&lt;tr&gt;&lt;td&gt;&lt;b&gt;Имя:&amp;nbsp;&amp;nbsp;&lt;/b&gt;&lt;/td&gt;&lt;td&gt;%2&lt;/td&gt;&lt;/tr&gt;&lt;tr&gt;&lt;td&gt;&lt;b&gt;Новая версия:&amp;nbsp;&amp;nbsp;&lt;/b&gt;&lt;/td&gt;&lt;td&gt;%3&lt;/td&gt;&lt;/tr&gt;&lt;tr&gt;&lt;td&gt;&lt;b&gt;Текущая версия:&amp;nbsp;&amp;nbsp;&lt;/b&gt;&lt;/td&gt;&lt;td&gt;%4&lt;/td&gt;&lt;/tr&gt;&lt;tr&gt;&lt;td&gt;&lt;b&gt;Описание:&amp;nbsp;&amp;nbsp;&lt;/b&gt;&lt;/td&gt;&lt;td&gt;%5&lt;/td&gt;&lt;/tr&gt;&lt;/table&gt;&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&amp;Downgrade</source>
        <translation>&amp;Откатить</translation>
    </message>
    <message>
        <source>&lt;p&gt;The extension pack is already installed with the same version, would you like reinstall it? &lt;p&gt;%1&lt;/p&gt;&lt;p&gt;&lt;table cellpadding=0 cellspacing=0&gt;&lt;tr&gt;&lt;td&gt;&lt;b&gt;Name:&amp;nbsp;&amp;nbsp;&lt;/b&gt;&lt;/td&gt;&lt;td&gt;%2&lt;/td&gt;&lt;/tr&gt;&lt;tr&gt;&lt;td&gt;&lt;b&gt;Version:&amp;nbsp;&amp;nbsp;&lt;/b&gt;&lt;/td&gt;&lt;td&gt;%3&lt;/td&gt;&lt;/tr&gt;&lt;tr&gt;&lt;td&gt;&lt;b&gt;Description:&amp;nbsp;&amp;nbsp;&lt;/b&gt;&lt;/td&gt;&lt;td&gt;%4&lt;/td&gt;&lt;/tr&gt;&lt;/table&gt;&lt;/p&gt;</source>
        <translation>&lt;p&gt;У Вас установлена та же версия плагина, желаете ли её переустановить?&lt;p&gt;%1&lt;/p&gt;&lt;p&gt;&lt;table cellpadding=0 cellspacing=0&gt;&lt;tr&gt;&lt;td&gt;&lt;b&gt;Имя:&amp;nbsp;&amp;nbsp;&lt;/b&gt;&lt;/td&gt;&lt;td&gt;%2&lt;/td&gt;&lt;/tr&gt;&lt;tr&gt;&lt;td&gt;&lt;b&gt;Версия:&amp;nbsp;&amp;nbsp;&lt;/b&gt;&lt;/td&gt;&lt;td&gt;%3&lt;/td&gt;&lt;/tr&gt;&lt;tr&gt;&lt;td&gt;&lt;b&gt;Описание:&amp;nbsp;&amp;nbsp;&lt;/b&gt;&lt;/td&gt;&lt;td&gt;%4&lt;/td&gt;&lt;/tr&gt;&lt;/table&gt;&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&amp;Reinstall</source>
        <translation>&amp;Переустановить</translation>
    </message>
    <message>
        <source>&lt;p&gt;You are about to remove the VirtualBox extension pack &lt;b&gt;%1&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Are you sure you want to proceed?&lt;/p&gt;</source>
        <translation>&lt;p&gt;Вы собираетесь удалить VirtualBox плагин &lt;b&gt;%1&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Вы уверены, что хотите продолжить?&lt;/p&gt;</translation>
    </message>
    <message>
        <source>The extension pack &lt;br&gt;&lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;nobr&gt;&lt;br&gt; was installed successfully.</source>
        <translation>Плагин &lt;br&gt;&lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;nobr&gt;&lt;br&gt; был успешно установлен.</translation>
    </message>
    <message>
        <source>Deletion of all files belonging to the VM is currently disabled on Windows/x64 to prevent a crash. That will be fixed in the next release.</source>
        <translation type="obsolete">Удаление файлов, принадлежащих виртуальной машине, в данный запрещено для ОС Windows/x64 (во избежание сбоя самой ОС). Данная проблема будет исправлена в будущем релизе.</translation>
    </message>
    <message>
        <source>&lt;p&gt;Cannot create the machine folder &lt;b&gt;%1&lt;/b&gt; in the parent folder &lt;nobr&gt;&lt;b&gt;%2&lt;/b&gt;.&lt;/nobr&gt;&lt;/p&gt;&lt;p&gt;Please check that the parent really exists and that you have permissions to create the machine folder.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Не удалось создать директорию машины &lt;b&gt;%1&lt;/b&gt; в родительском каталоге &lt;nobr&gt;&lt;b&gt;%2&lt;/b&gt;.&lt;/nobr&gt;&lt;/p&gt;&lt;p&gt;Пожалуйста, проверьте факт существования родительского каталога и наличие у Вас доступа для создания в нём папок и файлов.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;USB 2.0 is currently enabled for this virtual machine. However this requires the &lt;b&gt;&lt;nobr&gt;%1&lt;/nobr&gt;&lt;/b&gt; to be installed.&lt;/p&gt;&lt;p&gt;Please install the Extension Pack from the VirtualBox download site. After this you will be able to re-enable USB 2.0. It will be disabled in the meantime unless you cancel the current settings changes.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;В настоящий момент, поддержка USB 2.0 включена для данной виртуальной машины. Однако, для того, чтобы она работала верно, необходимо установить плагин &lt;b&gt;&lt;nobr&gt;%1&lt;/nobr&gt;&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Пожалуйста, установите плагин, предварительно загрузив его с сайта поддержки VirtualBox. После этого Вы сможете повторно активировать поддержку USB 2.0. До установки плагина поддержка USB 2.0 будет автоматически отключаться по принятии настроек данной машины.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Could not load the Host USB Proxy Service (VERR_FILE_NOT_FOUND). The service might not be installed on the host computer</source>
        <translation>Не удалось загрузить USB Proxy службу хоста (VERR_FILE_NOT_FOUND). Возможно, данная служба не установлена на данном компьютере</translation>
    </message>
    <message>
        <source>VirtualBox is not currently allowed to access USB devices.  You can change this by adding your user to the &apos;vboxusers&apos; group.  Please see the user manual for a more detailed explanation</source>
        <translation>В данный момент VirtualBox не может использовать устройства USB. Вы можете исправить данную проблему, добавив текущего пользователя в группу &apos;vboxusers&apos;. Для получения более детальных объяснений следуйте указаниям руководства пользователя</translation>
    </message>
    <message>
        <source>VirtualBox is not currently allowed to access USB devices.  You can change this by allowing your user to access the &apos;usbfs&apos; folder and files.  Please see the user manual for a more detailed explanation</source>
        <translation>В данный момент VirtualBox не может использовать устройства USB. Вы можете исправить данную проблему, открыв текущему пользователю доступ к файлам и папкам &apos;usbfs&apos;. Для получения более детальных объяснений следуйте указаниям руководства пользователя</translation>
    </message>
    <message>
        <source>The USB Proxy Service has not yet been ported to this host</source>
        <translation>Служба USB Proxy ещё не была портирована на данный хост</translation>
    </message>
    <message>
        <source>Could not load the Host USB Proxy service</source>
        <translation>Не удалось загрузить USB Proxy службу хоста</translation>
    </message>
    <message>
        <source>Failed to register the virtual machine &lt;b&gt;%1&lt;/b&gt;.</source>
        <translation>Не удалось зарегистрировать виртуальную машину &lt;b&gt;%1&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>&lt;p&gt;The machine settings were changed while you were editing them. You currently have unsaved setting changes.&lt;/p&gt;&lt;p&gt;Would you like to reload the changed settings or to keep your own changes?&lt;/p&gt;</source>
        <translation>&lt;p&gt;Настройки машины были изменены извне. У Вас в данный момент имеются несохранённые изменения.&lt;/p&gt;&lt;p&gt;Желаете загрузить новые настройки или оставить свои изменения?&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Reload settings</source>
        <translation>Загрузить новые</translation>
    </message>
    <message>
        <source>Keep changes</source>
        <translation>Оставить свои</translation>
    </message>
    <message>
        <source>The state of the virtual machine you currently edit has changed. Only settings which are editable at runtime are saved when you press OK. All changes to other settings will be lost.</source>
        <translation>Состояние машины, настройки которой Вы в данный момент изменяете, было изменено извне. В случае нажатия кнопки OK будут сохранены лишь те настройки, которые могут быть изменены во время работы машины. Все изменения других настроек будут утеряны.</translation>
    </message>
    <message>
        <source>Failed to clone the virtual machine &lt;b&gt;%1&lt;/b&gt;.</source>
        <translation>Не удалось скопировать виртуальную машину &lt;b&gt;%1&lt;/b&gt;.</translation>
    </message>
    <message>
        <source>&lt;p&gt;You are about to restore snapshot &lt;b&gt;%1&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;You can create a snapshot of the current state of the virtual machine first by checking the box below; if you do not do this the current state will be permanently lost. Do you wish to proceed?&lt;/p&gt;</source>
        <translation>&lt;p&gt;Вы собираетесь восстановить снимок &lt;b&gt;%1&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Перед этим Вы можете создать снимок текущего состояния машины, поставив галочку внизу, по желанию; если Вы этого не сделаете, текущее состояние будет утеряно навсегда. Хотите ли продолжить?&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Create a snapshot of the current machine state</source>
        <translation>Создать снимок текущего состояния машины</translation>
    </message>
    <message>
        <source>&lt;p&gt;Are you sure you want to restore snapshot &lt;b&gt;%1&lt;/b&gt;?&lt;/p&gt;</source>
        <translation>&lt;p&gt;Вы уверены, что хотите восстановить снимок &lt;b&gt;%1&lt;/b&gt;?&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Error changing medium type from &lt;b&gt;%1&lt;/b&gt; to &lt;b&gt;%2&lt;/b&gt;.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Не удалось сменить тип носителя с &lt;b&gt;&apos;%1&apos;&lt;/b&gt; на &lt;b&gt;&apos;%2&apos;&lt;/b&gt;.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;USB 2.0 is currently enabled for this virtual machine. However, this requires the &lt;b&gt;&lt;nobr&gt;%1&lt;/nobr&gt;&lt;/b&gt; to be installed.&lt;/p&gt;&lt;p&gt;Please install the Extension Pack from the VirtualBox download site. After this you will be able to re-enable USB 2.0. It will be disabled in the meantime unless you cancel the current settings changes.&lt;/p&gt;</source>
        <translation>&lt;p&gt;В настоящий момент, поддержка USB 2.0 включена для данной виртуальной машины. Однако, для того, чтобы она работала верно, необходимо установить плагин &lt;b&gt;&lt;nobr&gt;%1&lt;/nobr&gt;&lt;/b&gt;.&lt;/p&gt;&lt;p&gt;Пожалуйста, установите плагин, предварительно загрузив его с сайта поддержки VirtualBox. После этого Вы сможете повторно активировать поддержку USB 2.0. До установки плагина поддержка USB 2.0 будет автоматически отключаться по принятии настроек данной машины.&lt;/p&gt;</translation>
    </message>
</context>
<context>
    <name>VBoxRegistrationDlg</name>
    <message>
        <source>VirtualBox Registration Dialog</source>
        <translation type="obsolete">Диалог регистрации VirtualBox</translation>
    </message>
    <message>
        <source>&amp;Name</source>
        <translation type="obsolete">&amp;Имя</translation>
    </message>
    <message>
        <source>Enter your full name using Latin characters.</source>
        <translation type="obsolete">Впишите Ваше полное имя, используя символы латиницы.</translation>
    </message>
    <message>
        <source>&amp;E-mail</source>
        <translation type="obsolete">&amp;Электропочта</translation>
    </message>
    <message>
        <source>Enter your e-mail address. Please use a valid address here.</source>
        <translation type="obsolete">Впишите действительный адрес Вашей электронной почты.</translation>
    </message>
    <message>
        <source>&amp;Please do not use this information to contact me</source>
        <translation type="obsolete">&amp;Не использовать эту информацию для связи со мной</translation>
    </message>
    <message>
        <source>Welcome to the VirtualBox Registration Form!</source>
        <translation type="obsolete">Регистрационная форма VirtualBox</translation>
    </message>
    <message>
        <source>Could not perform connection handshake.</source>
        <translation type="obsolete">Не удалось установить соединение.</translation>
    </message>
    <message>
        <source>&lt;p&gt;Please fill out this registration form to let us know that you use VirtualBox and, optionally, to keep you informed about VirtualBox news and updates.&lt;/p&gt;&lt;p&gt;Enter your full name using Latin characters and your e-mail address to the fields below. Sun Microsystems will use this information only to gather product usage statistics and to send you VirtualBox newsletters. In particular, Sun Microsystems will never pass your data to third parties. Detailed information about how we use your personal data can be found in the &lt;b&gt;Privacy Policy&lt;/b&gt; section of the VirtualBox Manual or on the &lt;a href=http://www.virtualbox.org/wiki/PrivacyPolicy&gt;Privacy Policy&lt;/a&gt; page of the VirtualBox web-site.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Пожалуйста, заполните предлагаемую регистрационную форму. Тем самым, Вы известите нас о том, что пользуетесь нашим продуктом, а также получите возможность быть в курсе новостей и обновлений VirtualBox (по желанию).&lt;/p&gt;&lt;p&gt;Впишите Ваше полное имя, используя для этого символы латиницы, а также адрес электронной почты в расположенные ниже поля. Пожалуйста, примите к сведению, что эта информация будет использована только для сбора статистики о количестве пользователей и для рассылки новостей VirtualBox. Иными словами, Sun Microsystems никогда не передаст данные о Вас какой-либо третьей стороне. Более подробные сведения о том, как мы будем использовать Ваши личные данные, можно найти в разделе &lt;b&gt;Политика конфиденциальности&lt;/b&gt; (Privacy Policy) Руководства пользователя VirtualBox или на странице &lt;a href=http://www.virtualbox.org/wiki/PrivacyPolicy&gt;Privacy Policy&lt;/a&gt; веб-сайта VirtualBox.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Check this box if you do not want to receive mail from Sun Microsystems at the e-mail address specified above.</source>
        <translation type="obsolete">Поставьте галочку, если Вы не хотите получать почту от Sun Microsystems на указанный выше адрес электронной почты.</translation>
    </message>
    <message>
        <source>C&amp;onfirm</source>
        <translation type="obsolete">О&amp;тправить</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation type="obsolete">Отмена</translation>
    </message>
    <message>
        <source>Select Country/Territory</source>
        <translation type="obsolete">Укажите Страну/Территорию</translation>
    </message>
    <message>
        <source>&lt;p&gt;Please fill out this registration form to let us know that you use VirtualBox and, optionally, to keep you informed about VirtualBox news and updates.&lt;/p&gt;&lt;p&gt;Please use Latin characters only to fill in  the fields below. Sun Microsystems will use this information only to gather product usage statistics and to send you VirtualBox newsletters. In particular, Sun Microsystems will never pass your data to third parties. Detailed information about how we use your personal data can be found in the &lt;b&gt;Privacy Policy&lt;/b&gt; section of the VirtualBox Manual or on the &lt;a href=http://www.virtualbox.org/wiki/PrivacyPolicy&gt;Privacy Policy&lt;/a&gt; page of the VirtualBox web-site.&lt;/p&gt;</source>
        <translation type="obsolete">&lt;p&gt;Пожалуйста, заполните предлагаемую регистрационную форму. Тем самым, Вы известите нас о том, что пользуетесь нашим продуктом, а также получите возможность быть в курсе новостей и обновлений VirtualBox (по желанию).&lt;/p&gt;&lt;p&gt;Впишите Ваше полное имя, используя для этого символы латиницы, а также адрес электронной почты в расположенные ниже поля. Пожалуйста, примите к сведению, что эта информация будет использована только для сбора статистики о количестве пользователей и для рассылки новостей VirtualBox. Иными словами, Sun Microsystems никогда не передаст данные о Вас какой-либо третьей стороне. Более подробные сведения о том, как мы будем использовать Ваши личные данные, можно найти в разделе &lt;b&gt;Политика конфиденциальности&lt;/b&gt; (Privacy Policy) Руководства пользователя VirtualBox или на странице &lt;a href=http://www.virtualbox.org/wiki/PrivacyPolicy&gt;Privacy Policy&lt;/a&gt; веб-сайта VirtualBox.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>I &amp;already have a Sun Online account:</source>
        <translation type="obsolete">&amp;У меня уже имеется учётная запись Sun Online:</translation>
    </message>
    <message>
        <source>&amp;E-mail:</source>
        <translation type="obsolete">Адрес &amp;электронной почты:</translation>
    </message>
    <message>
        <source>&amp;Password:</source>
        <translation type="obsolete">&amp;Пароль:</translation>
    </message>
    <message>
        <source>I &amp;would like to create a new Sun Online account:</source>
        <translation type="obsolete">&amp;Я бы хотел зарегистрироваться, создав новую учётную запись Sun Online:</translation>
    </message>
    <message>
        <source>&amp;First Name:</source>
        <translation type="obsolete">&amp;Имя:</translation>
    </message>
    <message>
        <source>&amp;Last Name:</source>
        <translation type="obsolete">&amp;Фамилия:</translation>
    </message>
    <message>
        <source>&amp;Company:</source>
        <translation type="obsolete">&amp;Компания:</translation>
    </message>
    <message>
        <source>Co&amp;untry:</source>
        <translation type="obsolete">&amp;Страна:</translation>
    </message>
    <message>
        <source>E-&amp;mail:</source>
        <translation type="obsolete">Адрес э&amp;лектронной почты:</translation>
    </message>
    <message>
        <source>P&amp;assword:</source>
        <translation type="obsolete">П&amp;ароль:</translation>
    </message>
    <message>
        <source>Co&amp;nfirm Password:</source>
        <translation type="obsolete">П&amp;одтвердить пароль:</translation>
    </message>
    <message>
        <source>&amp;Register</source>
        <translation type="obsolete">&amp;Регистрация</translation>
    </message>
</context>
<context>
    <name>VBoxSFDialog</name>
    <message>
        <source>Shared Folders</source>
        <translation type="obsolete">Общие папки</translation>
    </message>
</context>
<context>
    <name>VBoxScreenshotViewer</name>
    <message>
        <source>Screenshot of %1 (%2)</source>
        <translation>Снимок экрана %1 (%2)</translation>
    </message>
    <message>
        <source>Click to view non-scaled screenshot.</source>
        <translation>Кликните мышкой для просмотра немасшатбированного снимка экрана.</translation>
    </message>
    <message>
        <source>Click to view scaled screenshot.</source>
        <translation>Кликните мышкой для просмотра масштабированного снимка экрана.</translation>
    </message>
</context>
<context>
    <name>VBoxSelectorWnd</name>
    <message>
        <source>VirtualBox OSE</source>
        <translation type="obsolete">VirtualBox OSE</translation>
    </message>
    <message>
        <source>&amp;Details</source>
        <translation type="obsolete">&amp;Детали</translation>
    </message>
    <message>
        <source>&amp;Preferences...</source>
        <comment>global settings</comment>
        <translation>&amp;Свойства...</translation>
    </message>
    <message>
        <source>Display the global settings dialog</source>
        <translation>Открыть диалог глобальных настроек</translation>
    </message>
    <message>
        <source>E&amp;xit</source>
        <translation>&amp;Выход</translation>
    </message>
    <message>
        <source>Close application</source>
        <translation>Закрыть приложение</translation>
    </message>
    <message>
        <source>&amp;New...</source>
        <translation>&amp;Создать...</translation>
    </message>
    <message>
        <source>Create a new virtual machine</source>
        <translation>Создать новую виртуальную машину</translation>
    </message>
    <message>
        <source>&amp;Settings...</source>
        <translation>С&amp;войства...</translation>
    </message>
    <message>
        <source>Configure the selected virtual machine</source>
        <translation>Настроить выбранную виртуальную машину</translation>
    </message>
    <message>
        <source>&amp;Delete</source>
        <translation type="obsolete">&amp;Удалить</translation>
    </message>
    <message>
        <source>Delete the selected virtual machine</source>
        <translation type="obsolete">Удалить выбранную виртуальную машину</translation>
    </message>
    <message>
        <source>D&amp;iscard</source>
        <translation type="obsolete">Сб&amp;росить</translation>
    </message>
    <message>
        <source>Discard the saved state of the selected virtual machine</source>
        <translation>Сбросить (удалить) сохраненное состояние выбранной виртуальной машины</translation>
    </message>
    <message>
        <source>&amp;Refresh</source>
        <translation type="obsolete">О&amp;бновить</translation>
    </message>
    <message>
        <source>Refresh the accessibility state of the selected virtual machine</source>
        <translation>Перепроверить доступность выбранной виртуальной машины</translation>
    </message>
    <message>
        <source>&amp;File</source>
        <translation type="obsolete">&amp;Файл</translation>
    </message>
    <message>
        <source>&amp;Help</source>
        <translation>Справк&amp;а</translation>
    </message>
    <message>
        <source>&amp;Snapshots</source>
        <translation type="obsolete">&amp;Снимки</translation>
    </message>
    <message>
        <source>D&amp;escription</source>
        <translation type="obsolete">&amp;Описание</translation>
    </message>
    <message>
        <source>D&amp;escription *</source>
        <translation type="obsolete">&amp;Описание *</translation>
    </message>
    <message>
        <source>S&amp;how</source>
        <translation>&amp;Показать</translation>
    </message>
    <message>
        <source>Switch to the window of the selected virtual machine</source>
        <translation>Переключиться в окно выбранной виртуальной машины</translation>
    </message>
    <message>
        <source>S&amp;tart</source>
        <translation>С&amp;тарт</translation>
    </message>
    <message>
        <source>Start the selected virtual machine</source>
        <translation>Начать выполнение выбранной виртуальной машины</translation>
    </message>
    <message>
        <source>&amp;Machine</source>
        <translation>&amp;Машина</translation>
    </message>
    <message>
        <source>Show &amp;Log...</source>
        <translation>Показать &amp;журнал...</translation>
    </message>
    <message>
        <source>Show the log files of the selected virtual machine</source>
        <translation>Показать файлы журналов выбранной виртуальной машины</translation>
    </message>
    <message>
        <source>R&amp;esume</source>
        <translation type="obsolete">П&amp;родолжить</translation>
    </message>
    <message>
        <source>Resume the execution of the virtual machine</source>
        <translation>Возобновить работу приостановленной виртуальной машины</translation>
    </message>
    <message>
        <source>&amp;Pause</source>
        <translation>&amp;Пауза</translation>
    </message>
    <message>
        <source>Suspend the execution of the virtual machine</source>
        <translation>Приостановить работу виртуальной машины</translation>
    </message>
    <message>
        <source>&lt;h3&gt;Welcome to VirtualBox!&lt;/h3&gt;&lt;p&gt;The left part of this window is  a list of all virtual machines on your computer. The list is empty now because you haven&apos;t created any virtual machines yet.&lt;img src=:/welcome.png align=right/&gt;&lt;/p&gt;&lt;p&gt;In order to create a new virtual machine, press the &lt;b&gt;New&lt;/b&gt; button in the main tool bar located at the top of the window.&lt;/p&gt;&lt;p&gt;You can press the &lt;b&gt;%1&lt;/b&gt; key to get instant help, or visit &lt;a href=http://www.virtualbox.org&gt;www.virtualbox.org&lt;/a&gt; for the latest information and news.&lt;/p&gt;</source>
        <translation>&lt;h3&gt;Добро пожаловать в мир VirtualBox!&lt;/h3&gt;&lt;p&gt;Левая часть этого окна предназначена для отображения списка Ваших  виртуальных машин. Этот список сейчас пуст, потому что Вы не создали ни одной виртуальной машины.&lt;img src=:/welcome.png align=right/&gt;&lt;/p&gt;&lt;p&gt;Чтобы создать новую машину, нажмите кнопку &lt;b&gt;Создать&lt;/b&gt; на основной панели инструментов, расположенной вверху окна.&lt;/p&gt;&lt;p&gt;Hажмите клавишу &lt;b&gt;%1&lt;/b&gt; для получения оперативной помощи или посетите сайт &lt;a href=http://www.virtualbox.org&gt;www.virtualbox.org&lt;/a&gt;, чтобы узнать свежие новости и получить актуальную информацию.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&amp;Virtual Media Manager...</source>
        <translation>&amp;Менеджер виртуальных носителей...</translation>
    </message>
    <message>
        <source>Display the Virtual Media Manager dialog</source>
        <translation>Открыть диалог Менеджера виртуальных носителей</translation>
    </message>
    <message>
        <source>Log</source>
        <comment>icon text</comment>
        <translation>Журнал</translation>
    </message>
    <message>
        <source>Sun VirtualBox</source>
        <translation type="obsolete">Sun VirtualBox</translation>
    </message>
    <message>
        <source>&amp;Import Appliance...</source>
        <translation>&amp;Импорт конфигурации...</translation>
    </message>
    <message>
        <source>Import an appliance into VirtualBox</source>
        <translation>Импорт внешней конфигурации группы виртуальных машин в VirtualBox</translation>
    </message>
    <message>
        <source>&amp;Export Appliance...</source>
        <translation>&amp;Экспорт конфигурации...</translation>
    </message>
    <message>
        <source>Export one or more VirtualBox virtual machines as an appliance</source>
        <translation>Экспорт конфигурации группы виртуальных машин из VirtualBox</translation>
    </message>
    <message>
        <source>Re&amp;fresh</source>
        <translation>Об&amp;новить</translation>
    </message>
    <message>
        <source>&amp;File</source>
        <comment>Mac OS X version</comment>
        <translation>&amp;Файл</translation>
    </message>
    <message>
        <source>&amp;File</source>
        <comment>Non Mac OS X version</comment>
        <translation>&amp;Файл</translation>
    </message>
    <message>
        <source>Select a virtual machine file</source>
        <translation>Выберите файл виртуальной машины</translation>
    </message>
    <message>
        <source>Virtual machine files (%1)</source>
        <translation>Файлы виртуальных машин (%1)</translation>
    </message>
    <message>
        <source>Manager</source>
        <comment>Note: main window title which is pretended by the product name.</comment>
        <translation>Менеджер</translation>
    </message>
    <message>
        <source>&amp;Add...</source>
        <translation>&amp;Добавить...</translation>
    </message>
    <message>
        <source>Add an existing virtual machine</source>
        <translation>Добавить существующую виртуальную машину</translation>
    </message>
    <message>
        <source>&amp;Remove</source>
        <translation>&amp;Убрать</translation>
    </message>
    <message>
        <source>Remove the selected virtual machine</source>
        <translation>Убрать выбранную виртуальную машину</translation>
    </message>
    <message>
        <source>Show in Finder</source>
        <translation>Показать в поисковике</translation>
    </message>
    <message>
        <source>Show the VirtualBox Machine Definition file in Finder.</source>
        <translation>Показать файл виртуальной машины VirtualBox в поисковике.</translation>
    </message>
    <message>
        <source>Create Alias on Desktop</source>
        <translation>Создать ярлык на рабочем столе</translation>
    </message>
    <message>
        <source>Creates an Alias file to the VirtualBox Machine Definition file on your Desktop.</source>
        <translation>Создать ярлык виртуальной машины VirtualBox на Вашем рабочем столе.</translation>
    </message>
    <message>
        <source>Show in Explorer</source>
        <translation>Показать в обозревателе</translation>
    </message>
    <message>
        <source>Show the VirtualBox Machine Definition file in Explorer.</source>
        <translation>Показать файл виртуальной машины VirtualBox в обозревателе.</translation>
    </message>
    <message>
        <source>Create Shortcut on Desktop</source>
        <translation>Создать ярлык на рабочем столе</translation>
    </message>
    <message>
        <source>Creates an Shortcut file to the VirtualBox Machine Definition file on your Desktop.</source>
        <translation>Создать ярлык виртуальной машины VirtualBox на Вашем рабочем столе.</translation>
    </message>
    <message>
        <source>Show in File Manager</source>
        <translation>Показать в файловом менеджере</translation>
    </message>
    <message>
        <source>Show the VirtualBox Machine Definition file in the File Manager</source>
        <translation>Показать файл виртуальной машины VirtualBox в файловом менеджере.</translation>
    </message>
    <message>
        <source>Show Toolbar</source>
        <translation>Показать тулбар</translation>
    </message>
    <message>
        <source>Show Statusbar</source>
        <translation>Показать строку статуса</translation>
    </message>
    <message>
        <source>Cl&amp;one...</source>
        <translation>&amp;Копировать...</translation>
    </message>
    <message>
        <source>Clone the selected virtual machine</source>
        <translation>Копировать выбранную виртуальную машину</translation>
    </message>
    <message>
        <source>Discard</source>
        <translation>Сбросить</translation>
    </message>
    <message>
        <source>D&amp;iscard Saved State</source>
        <translation>С&amp;бросить сохранённое состояние</translation>
    </message>
</context>
<context>
    <name>VBoxSettingsDialog</name>
    <message>
        <source>&lt;i&gt;Select a settings category from the list on the left-hand side and move the mouse over a settings item to get more information&lt;/i&gt;.</source>
        <translation type="obsolete">&lt;i&gt;Выберите раздел настроек из списка слева, после чего поместите курсор мыши над нужным элементом настроек для получения подробной информации&lt;i&gt;.</translation>
    </message>
    <message>
        <source>Invalid settings detected</source>
        <translation type="obsolete">Обнаружены неправильные настройки</translation>
    </message>
    <message>
        <source>Settings</source>
        <translation type="obsolete">Свойства</translation>
    </message>
    <message>
        <source>Non-optimal settings detected</source>
        <translation type="obsolete">Обнаружены неоптимальные настройки</translation>
    </message>
    <message>
        <source>On the &lt;b&gt;%1&lt;/b&gt; page, %2</source>
        <translation type="obsolete">На странице &lt;b&gt;&apos;%1&apos;&lt;/b&gt;: %2</translation>
    </message>
</context>
<context>
    <name>VBoxSnapshotDetailsDlg</name>
    <message>
        <source>&amp;Name</source>
        <translation type="obsolete">&amp;Имя</translation>
    </message>
    <message>
        <source>&amp;Description</source>
        <translation type="obsolete">О&amp;писание</translation>
    </message>
    <message>
        <source>&amp;Machine Details</source>
        <translation type="obsolete">Сведения о &amp;машине</translation>
    </message>
    <message>
        <source>Details of %1 (%2)</source>
        <translation>Сведения о %1 (%2)</translation>
    </message>
    <message>
        <source>Snapshot Details</source>
        <translation type="obsolete"> Сведения о снимке </translation>
    </message>
    <message>
        <source>Click to enlarge the screenshot.</source>
        <translation>Кликните мышкой для увеличения снимка экрана.</translation>
    </message>
    <message>
        <source>&amp;Name:</source>
        <translation>&amp;Имя:</translation>
    </message>
    <message>
        <source>Taken:</source>
        <translation>Создан:</translation>
    </message>
    <message>
        <source>&amp;Description:</source>
        <translation>&amp;Описание:</translation>
    </message>
    <message>
        <source>D&amp;etails:</source>
        <translation>&amp;Детали:</translation>
    </message>
</context>
<context>
    <name>VBoxSnapshotsWgt</name>
    <message>
        <source>VBoxSnapshotsWgt</source>
        <translation></translation>
    </message>
    <message>
        <source>&amp;Discard Snapshot</source>
        <translation type="obsolete">Cб&amp;росить снимок</translation>
    </message>
    <message>
        <source>Take &amp;Snapshot</source>
        <translation>&amp;Сделать снимок</translation>
    </message>
    <message>
        <source>D&amp;iscard Current Snapshot and State</source>
        <translation type="obsolete">Сб&amp;росить текущий снимок и состояние</translation>
    </message>
    <message>
        <source>S&amp;how Details</source>
        <translation>&amp;Показать детали</translation>
    </message>
    <message>
        <source>Current State (changed)</source>
        <comment>Current State (Modified)</comment>
        <translation>Текущее состояние (изменено)</translation>
    </message>
    <message>
        <source>Current State</source>
        <comment>Current State (Unmodified)</comment>
        <translation>Текущее состояние</translation>
    </message>
    <message>
        <source>The current state differs from the state stored in the current snapshot</source>
        <translation>Текущее состояние отличается от состояния, сохраненного в текущем снимке</translation>
    </message>
    <message>
        <source>The current state is identical to the state stored in the current snapshot</source>
        <translation>Текущее состояние идентично состоянию, сохраненному в текущем снимке</translation>
    </message>
    <message>
        <source> (current, </source>
        <comment>Snapshot details</comment>
        <translation> (текущий, </translation>
    </message>
    <message>
        <source>online)</source>
        <comment>Snapshot details</comment>
        <translation>с работающей машины)</translation>
    </message>
    <message>
        <source>offline)</source>
        <comment>Snapshot details</comment>
        <translation>с выключенной машины)</translation>
    </message>
    <message>
        <source>Taken at %1</source>
        <comment>Snapshot (time)</comment>
        <translation>Сделан в %1</translation>
    </message>
    <message>
        <source>Taken on %1</source>
        <comment>Snapshot (date + time)</comment>
        <translation>Сделан %1</translation>
    </message>
    <message>
        <source>%1 since %2</source>
        <comment>Current State (time or date + time)</comment>
        <translation>%1 с %2</translation>
    </message>
    <message>
        <source>Snapshot %1</source>
        <translation>Снимок %1</translation>
    </message>
    <message>
        <source>Discard the selected snapshot of the virtual machine</source>
        <translation type="obsolete">Сбросить (удалить) выбранный снимок виртуальной машины</translation>
    </message>
    <message>
        <source>Take a snapshot of the current virtual machine state</source>
        <translation>Сделать снимок текущего состояния виртуальной машины</translation>
    </message>
    <message>
        <source>&amp;Revert to Current Snapshot</source>
        <translation type="obsolete">&amp;Вернуться к текущему снимку</translation>
    </message>
    <message>
        <source>Restore the virtual machine state from the state stored in the current snapshot</source>
        <translation type="obsolete">Восстановить состояние виртуальной машины из состояния, сохраненного в текущем снимке</translation>
    </message>
    <message>
        <source>Discard the current snapshot and revert the machine to the state it had before the snapshot was taken</source>
        <translation type="obsolete">Сбросить (удалить) текущий снимок и вернуть машину к состоянию, в котором она была перед его созданием</translation>
    </message>
    <message>
        <source>Show the details of the selected snapshot</source>
        <translation>Показать подробности о выбранном снимке</translation>
    </message>
    <message>
        <source> (%1)</source>
        <translation> (%1)</translation>
    </message>
    <message numerus="yes">
        <source> (%n day(s) ago)</source>
        <translation type="obsolete">
            <numerusform> (%n день назад)</numerusform>
            <numerusform> (%n дня назад)</numerusform>
            <numerusform> (%n дней назад)</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source> (%n hour(s) ago)</source>
        <translation type="obsolete">
            <numerusform> (%n час назад)</numerusform>
            <numerusform> (%n часа назад)</numerusform>
            <numerusform> (%n часов назад)</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source> (%n minute(s) ago)</source>
        <translation type="obsolete">
            <numerusform> (%n минута назад)</numerusform>
            <numerusform> (%n минуты назад)</numerusform>
            <numerusform> (%n минут назад)</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source> (%n second(s) ago)</source>
        <translation type="obsolete">
            <numerusform> (%n секунда назад)</numerusform>
            <numerusform> (%n секунды назад)</numerusform>
            <numerusform> (%n секунд назад)</numerusform>
        </translation>
    </message>
    <message>
        <source>&amp;Restore Snapshot</source>
        <translation>&amp;Восстановить снимок</translation>
    </message>
    <message>
        <source>&amp;Delete Snapshot</source>
        <translation>&amp;Удалить снимок</translation>
    </message>
    <message>
        <source>Restore the selected snapshot of the virtual machine</source>
        <translation>Восстановить выбранный снимок виртуальной машины</translation>
    </message>
    <message>
        <source>Delete the selected snapshot of the virtual machine</source>
        <translation>Удалить выбранный снимок виртуальной машины</translation>
    </message>
    <message>
        <source> (%1 ago)</source>
        <translation> (%1 назад)</translation>
    </message>
    <message>
        <source>&amp;Clone...</source>
        <translation>&amp;Копировать...</translation>
    </message>
    <message>
        <source>Clone the selected virtual machine</source>
        <translation>Копировать выбранную виртуальную машину</translation>
    </message>
</context>
<context>
    <name>VBoxSwitchMenu</name>
    <message>
        <source>Disable</source>
        <translation>Отключить</translation>
    </message>
    <message>
        <source>Enable</source>
        <translation>Включить</translation>
    </message>
</context>
<context>
    <name>VBoxTakeSnapshotDlg</name>
    <message>
        <source>Take Snapshot of Virtual Machine</source>
        <translation>Сделать снимок виртуальной машины</translation>
    </message>
    <message>
        <source>Snapshot &amp;Name</source>
        <translation>&amp;Имя снимка</translation>
    </message>
    <message>
        <source>Snapshot &amp;Description</source>
        <translation>О&amp;писание снимка</translation>
    </message>
    <message numerus="yes">
        <source>Warning: You are taking a snapshot of a running machine which has %n immutable image(s) attached to it. As long as you are working from this snapshot the immutable image(s) will not be reset to avoid loss of data.</source>
        <translation>
            <numerusform>Предупреждение: Вы собираетесь взять снимок машины, активной в данный момент и имеющей %n неизменяемый образ, подсоединённый к ней. До тех пор пока Вы работаете в данном снимке, неизменяемый образ не будет сброшен, дабы не допустить потерю данных.</numerusform>
            <numerusform>Предупреждение: Вы собираетесь взять снимок машины, активной в данный момент и имеющей %n неизменяемых образа, подсоединённых к ней. До тех пор пока Вы работаете в данном снимке, неизменяемые образы не будут сброшены, дабы не допустить потерю данных.</numerusform>
            <numerusform>Предупреждение: Вы собираетесь взять снимок машины, активной в данный момент и имеющей %n неизменяемых образов, подсоединённых к ней. До тех пор пока Вы работаете в данном снимке, неизменяемые образы не будут сброшены, дабы не допустить потерю данных.</numerusform>
        </translation>
    </message>
</context>
<context>
    <name>VBoxTextEditor</name>
    <message>
        <source>Edit text</source>
        <translation>Измените текст</translation>
    </message>
    <message>
        <source>&amp;Replace...</source>
        <translation>&amp;Открыть...</translation>
    </message>
    <message>
        <source>Replaces the current text with the content of a file.</source>
        <translation>Заменить имеющийся текст содержимым текстового файла.</translation>
    </message>
    <message>
        <source>Text (*.txt);;All (*.*)</source>
        <translation>Текстовый файл (*.txt);;Любой файл (*.*)</translation>
    </message>
    <message>
        <source>Select a file to open...</source>
        <translation>Выберите файл...</translation>
    </message>
</context>
<context>
    <name>VBoxTrayIcon</name>
    <message>
        <source>Show Selector Window</source>
        <translation type="obsolete">Показать главное окно</translation>
    </message>
    <message>
        <source>Show the selector window assigned to this menu</source>
        <translation type="obsolete">Показать главное окно VirtualBox со списком виртуальных машин</translation>
    </message>
    <message>
        <source>Hide Tray Icon</source>
        <translation type="obsolete">Убрать значок из трея</translation>
    </message>
    <message>
        <source>Remove this icon from the system tray</source>
        <translation type="obsolete">Убрать этот значок из системного трея</translation>
    </message>
    <message>
        <source>&amp;Other Machines...</source>
        <comment>tray menu</comment>
        <translation type="obsolete">&amp;Еще машины...</translation>
    </message>
</context>
<context>
    <name>VBoxUSBMenu</name>
    <message>
        <source>&lt;no devices available&gt;</source>
        <comment>USB devices</comment>
        <translation>&lt;нет доступных устройств&gt;</translation>
    </message>
    <message>
        <source>No supported devices connected to the host PC</source>
        <comment>USB device tooltip</comment>
        <translation>Нет поддерживаемых устройств, подключенных к основному ПК</translation>
    </message>
</context>
<context>
    <name>VBoxUpdateDlg</name>
    <message>
        <source>1 day</source>
        <translation>1 день</translation>
    </message>
    <message>
        <source>2 days</source>
        <translation>2 дня</translation>
    </message>
    <message>
        <source>3 days</source>
        <translation>3 дня</translation>
    </message>
    <message>
        <source>4 days</source>
        <translation>4 дня</translation>
    </message>
    <message>
        <source>5 days</source>
        <translation>5 дней</translation>
    </message>
    <message>
        <source>6 days</source>
        <translation>6 дней</translation>
    </message>
    <message>
        <source>1 week</source>
        <translation>1 неделю</translation>
    </message>
    <message>
        <source>2 weeks</source>
        <translation>2 недели</translation>
    </message>
    <message>
        <source>3 weeks</source>
        <translation>3 недели</translation>
    </message>
    <message>
        <source>1 month</source>
        <translation>1 месяц</translation>
    </message>
    <message>
        <source>Never</source>
        <translation>Никогда</translation>
    </message>
    <message>
        <source>Chec&amp;k</source>
        <translation>&amp;Проверить</translation>
    </message>
    <message>
        <source>&amp;Close</source>
        <translation>&amp;Закрыть</translation>
    </message>
    <message>
        <source>VirtualBox Update Wizard</source>
        <translation>Мастер обновления VirtualBox</translation>
    </message>
    <message>
        <source>Check for Updates</source>
        <translation>Проверить обновления</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation>Отмена</translation>
    </message>
    <message>
        <source>Summary</source>
        <translation>Итог</translation>
    </message>
    <message>
        <source>&lt;p&gt;A new version of VirtualBox has been released! Version &lt;b&gt;%1&lt;/b&gt; is available at &lt;a href=&quot;http://www.virtualbox.org/&quot;&gt;virtualbox.org&lt;/a&gt;.&lt;/p&gt;&lt;p&gt;You can download this version using the link:&lt;/p&gt;&lt;p&gt;&lt;a href=%2&gt;%3&lt;/a&gt;&lt;/p&gt;</source>
        <translation>&lt;p&gt;Выпущена новая версия программы VirtualBox! Версия &lt;b&gt;%1&lt;/b&gt; доступна на сайте &lt;a href=&quot;http://www.virtualbox.org/&quot;&gt;virtualbox.org&lt;/a&gt;.&lt;/p&gt;&lt;p&gt;Вы можете скачать эту версию, используя следующую прямую ссылку: &lt;/p&gt;&lt;p&gt;&lt;a href=%2&gt;%3&lt;/a&gt;&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Unable to obtain the new version information due to the following network error:&lt;/p&gt;&lt;p&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/p&gt;</source>
        <translation>&lt;p&gt;Невозможно получить информацию о версии из-за следующей сетевой ошибки: &lt;/p&gt;&lt;p&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/p&gt;</translation>
    </message>
    <message>
        <source>You are already running the most recent version of VirtualBox.</source>
        <translation>Вы уже установили последнюю версию программы VirtualBox. Повторите проверку обновлений позже.</translation>
    </message>
    <message>
        <source>&lt;p&gt;This wizard will connect to the VirtualBox web-site and check if a newer version of VirtualBox is available.&lt;/p&gt;&lt;p&gt;Use the &lt;b&gt;Check&lt;/b&gt; button to check for a new version now or the &lt;b&gt;Cancel&lt;/b&gt; button if you do not want to perform this check.&lt;/p&gt;&lt;p&gt;You can run this wizard at any time by choosing &lt;b&gt;Check for Updates...&lt;/b&gt; from the &lt;b&gt;Help&lt;/b&gt; menu.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Этот мастер попробует соединиться с веб-сайтом VirtualBox и проверить, не выпущена ли новая версия приложения VirtualBox.&lt;/p&gt;&lt;p&gt;Используйте кнопку &lt;b&gt;Проверить&lt;/b&gt; для выполнения проверки прямо сейчас или кнопку &lt;b&gt;Отмена&lt;/b&gt;, если Вы не хотите выполнять проверку.&lt;/p&gt;&lt;p&gt;Вы можете вызвать мастера обновлений в любое время с помощью пункта &lt;b&gt;Проверить обновления...&lt;/b&gt; в меню &lt;b&gt;Справка&lt;/b&gt;.&lt;/p&gt;</translation>
    </message>
</context>
<context>
    <name>VBoxVMDescriptionPage</name>
    <message>
        <source>No description. Press the Edit button below to add it.</source>
        <translation type="obsolete">Описание отсутствует. Чтобы его добавить, нажмите кнопку &lt;b&gt;Изменить&lt;/b&gt; внизу окна.</translation>
    </message>
    <message>
        <source>Edit</source>
        <translation type="obsolete">Изменить</translation>
    </message>
    <message>
        <source>Edit (Ctrl+E)</source>
        <translation type="obsolete">Изменить (Ctrl+E)</translation>
    </message>
</context>
<context>
    <name>VBoxVMDetailsView</name>
    <message>
        <source>The selected virtual machine is &lt;i&gt;inaccessible&lt;/i&gt;. Please inspect the error message shown below and press the &lt;b&gt;Refresh&lt;/b&gt; button if you want to repeat the accessibility check:</source>
        <translation type="obsolete">Выбранная виртуальная машина &lt;i&gt;недоступна&lt;/i&gt;. Внимательно просмотрите приведенное ниже сообщение об ошибке и нажмите кнопку &lt;b&gt;Обновить&lt;/b&gt;, если Вы хотите повторить проверку доступности:</translation>
    </message>
</context>
<context>
    <name>VBoxVMInformationDlg</name>
    <message>
        <source>%1 - Session Information</source>
        <translation>%1 - Информация о сессии</translation>
    </message>
    <message>
        <source>&amp;Details</source>
        <translation>&amp;Детали</translation>
    </message>
    <message>
        <source>&amp;Runtime</source>
        <translation>&amp;Работа</translation>
    </message>
    <message>
        <source>DMA Transfers</source>
        <translation>Запросов DMA</translation>
    </message>
    <message>
        <source>PIO Transfers</source>
        <translation>Запросов PIO</translation>
    </message>
    <message>
        <source>Data Read</source>
        <translation>Данных считано</translation>
    </message>
    <message>
        <source>Data Written</source>
        <translation>Данных записано</translation>
    </message>
    <message>
        <source>Data Transmitted</source>
        <translation>Данных передано</translation>
    </message>
    <message>
        <source>Data Received</source>
        <translation>Данных принято</translation>
    </message>
    <message>
        <source>Runtime Attributes</source>
        <translation>Рабочие характеристики</translation>
    </message>
    <message>
        <source>Screen Resolution</source>
        <translation>Разрешение экрана</translation>
    </message>
    <message>
        <source>CD/DVD-ROM Statistics</source>
        <translation type="obsolete">Статистика CD/DVD-ROM</translation>
    </message>
    <message>
        <source>Network Adapter Statistics</source>
        <translation type="obsolete">Статистика сетевых адаптеров</translation>
    </message>
    <message>
        <source>Version %1.%2</source>
        <comment>guest additions</comment>
        <translation type="obsolete">Версия %1.%2</translation>
    </message>
    <message>
        <source>Not Detected</source>
        <comment>guest additions</comment>
        <translation>Не обнаружены</translation>
    </message>
    <message>
        <source>Not Detected</source>
        <comment>guest os type</comment>
        <translation>Не определен</translation>
    </message>
    <message>
        <source>Guest Additions</source>
        <translation>Дополнения гостевой ОС</translation>
    </message>
    <message>
        <source>Guest OS Type</source>
        <translation>Тип гостевой ОС</translation>
    </message>
    <message>
        <source>Hard Disk Statistics</source>
        <translation type="obsolete">Статистика жестких дисков</translation>
    </message>
    <message>
        <source>No Hard Disks</source>
        <translation type="obsolete">Нет жестких дисков</translation>
    </message>
    <message>
        <source>No Network Adapters</source>
        <translation>Нет сетевых адаптеров</translation>
    </message>
    <message>
        <source>Enabled</source>
        <comment>nested paging</comment>
        <translation type="obsolete">Включена</translation>
    </message>
    <message>
        <source>Disabled</source>
        <comment>nested paging</comment>
        <translation type="obsolete">Выключена</translation>
    </message>
    <message>
        <source>Nested Paging</source>
        <translation type="obsolete">Функция Nested Paging</translation>
    </message>
    <message>
        <source>VBoxVMInformationDlg</source>
        <translation></translation>
    </message>
    <message>
        <source>Not Available</source>
        <comment>details report (VRDP server port)</comment>
        <translation type="obsolete">Не доступен</translation>
    </message>
    <message>
        <source>Storage Statistics</source>
        <translation>Статистика носителей</translation>
    </message>
    <message>
        <source>No Storage Devices</source>
        <translation>Нет носителей информации</translation>
    </message>
    <message>
        <source>Network Statistics</source>
        <translation>Сетевая статистика</translation>
    </message>
    <message>
        <source>Not Available</source>
        <comment>details report (VRDE server port)</comment>
        <translation>Не доступен</translation>
    </message>
</context>
<context>
    <name>VBoxVMLogViewer</name>
    <message>
        <source>Log Viewer</source>
        <translation>Просмотр журналов</translation>
    </message>
    <message>
        <source>&amp;Save</source>
        <translation>&amp;Сохранить</translation>
    </message>
    <message>
        <source>&amp;Refresh</source>
        <translation>О&amp;бновить</translation>
    </message>
    <message>
        <source>%1 - VirtualBox Log Viewer</source>
        <translation>%1 - Просмотр журналов VirtualBox</translation>
    </message>
    <message>
        <source>&lt;p&gt;No log files found. Press the &lt;b&gt;Refresh&lt;/b&gt; button to rescan the log folder &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/nobr&gt;.&lt;/p&gt;</source>
        <translation>&lt;p&gt;Файлы журналов не найдены. Нажмите кнопку &lt;b&gt;Обновить&lt;/b&gt; для того, чтобы перечитать содержимое папки &lt;nobr&gt;&lt;b&gt;%1&lt;/b&gt;&lt;/nobr&gt;.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Save VirtualBox Log As</source>
        <translation>Сохранить журнал VirtualBox как</translation>
    </message>
    <message>
        <source>&amp;Find</source>
        <translation>&amp;Найти</translation>
    </message>
    <message>
        <source>Close</source>
        <translation>Закрыть</translation>
    </message>
</context>
<context>
    <name>VBoxVMSettingsCD</name>
    <message>
        <source>Host CD/DVD drive is not selected</source>
        <translation type="obsolete">Не выбран физический CD/DVD-привод</translation>
    </message>
    <message>
        <source>CD/DVD image file is not selected</source>
        <translation type="obsolete">Не выбран файл образа CD/DVD</translation>
    </message>
    <message>
        <source>When checked, mounts the specified media to the CD/DVD drive of the virtual machine. Note that the CD/DVD drive is always connected to the Secondary Master IDE controller of the machine.</source>
        <translation type="obsolete">Когда стоит галочка, подключает указанный носитель к приводу CD/DVD виртуальной машины. Обратите внимание, что привод CD/DVD всегда подсоединен к мастер-раззему вторичного IDE-контроллера машины.</translation>
    </message>
    <message>
        <source>&amp;Mount CD/DVD Drive</source>
        <translation type="obsolete">&amp;Подключить CD/DVD</translation>
    </message>
    <message>
        <source>Mounts the specified CD/DVD drive to the virtual CD/DVD drive.</source>
        <translation type="obsolete">Подключает указанный CD/DVD-привод к виртуальному CD/DVD-приводу.</translation>
    </message>
    <message>
        <source>Host CD/DVD &amp;Drive</source>
        <translation type="obsolete">&amp;Физический CD/DVD-привод</translation>
    </message>
    <message>
        <source>Lists host CD/DVD drives available to mount to the virtual machine.</source>
        <translation type="obsolete">Показывает список физических CD/DVD-приводов, доступных для подключения к виртуальной машине.</translation>
    </message>
    <message>
        <source>When checked, allows the guest to send ATAPI commands directly to the host drive which makes it possible to use CD/DVD writers connected to the host inside the VM. Note that writing audio CD inside the VM is not yet supported.</source>
        <translation type="obsolete">Когда стоит галочка, гостевой ОС разрешается посылать ATAPI-команды напрямую в физический привод, что делает возможным использовать подключенные к основному ПК устройства для записи CD/DVD внутри ВМ. Имейте в виду, что запись аудио-CD внутри ВМ пока еще не поддерживается.</translation>
    </message>
    <message>
        <source>Enable &amp;Passthrough</source>
        <translation type="obsolete">&amp;Включить прямой доступ</translation>
    </message>
    <message>
        <source>Mounts the specified CD/DVD image to the virtual CD/DVD drive.</source>
        <translation type="obsolete">Подключает указанный файл образа CD/DVD к виртуальному CD/DVD-приводу.</translation>
    </message>
    <message>
        <source>&amp;ISO Image File</source>
        <translation type="obsolete">Фа&amp;йл ISO-образа</translation>
    </message>
    <message>
        <source>Displays the image file to mount to the virtual CD/DVD drive and allows to quickly select a different image.</source>
        <translation type="obsolete">Показывает файл образа для подключения к виртуальному CD/DVD-приводу и позволяет быстро выбрать другой файл образа.</translation>
    </message>
    <message>
        <source>Invokes the Virtual Media Manager to select a CD/DVD image to mount.</source>
        <translation type="obsolete">Открывает диалог Менеджера виртуальных носителей для выбора файла образа.</translation>
    </message>
</context>
<context>
    <name>VBoxVMSettingsDlg</name>
    <message>
        <source>General</source>
        <translation type="obsolete">Общие</translation>
    </message>
    <message>
        <source>Storage</source>
        <translation type="obsolete">Носители</translation>
    </message>
    <message>
        <source>Hard Disks</source>
        <translation type="obsolete">Жесткие диски</translation>
    </message>
    <message>
        <source>CD/DVD-ROM</source>
        <translation type="obsolete">CD/DVD-ROM</translation>
    </message>
    <message>
        <source>Floppy</source>
        <translation type="obsolete">Дискета</translation>
    </message>
    <message>
        <source>Audio</source>
        <translation type="obsolete">Аудио</translation>
    </message>
    <message>
        <source>Network</source>
        <translation type="obsolete">Сеть</translation>
    </message>
    <message>
        <source>Ports</source>
        <translation type="obsolete">Порты</translation>
    </message>
    <message>
        <source>Serial Ports</source>
        <translation type="obsolete">COM-порты</translation>
    </message>
    <message>
        <source>Parallel Ports</source>
        <translation type="obsolete">LPT-порты</translation>
    </message>
    <message>
        <source>USB</source>
        <translation type="obsolete">USB</translation>
    </message>
    <message>
        <source>Shared Folders</source>
        <translation type="obsolete">Общие папки</translation>
    </message>
    <message>
        <source>Remote Display</source>
        <translation type="obsolete">Удаленный дисплей</translation>
    </message>
    <message>
        <source>On the &lt;b&gt;%1&lt;/b&gt; page, %2</source>
        <translation type="obsolete">На странице &lt;b&gt;%1&lt;/b&gt; %2</translation>
    </message>
    <message>
        <source>System</source>
        <translation type="obsolete">Система</translation>
    </message>
    <message>
        <source>Display</source>
        <translation type="obsolete">Дисплей</translation>
    </message>
    <message>
        <source>you have selected a 64-bit guest OS type for this VM. As such guests require hardware virtualization (VT-x/AMD-V), this feature will be enabled automatically.</source>
        <translation type="obsolete">для этой машины выбран 64-битный тип гостевой ОС. В связи с тем, что такие гостевые ОС требуют активации функций аппаратной виртуализации (VT-x/AMD-V), эти функции будут включены автоматически.</translation>
    </message>
    <message>
        <source>you have selected a 64-bit guest OS type for this VM. VirtualBox does not currently support more than one virtual CPU for 64-bit guests executed on 32-bit hosts.</source>
        <translation type="obsolete">для этой машины выбран 64-битный тип гостевой ОС. VirtualBox в настоящий момент не поддерживает более одного виртуального процессора для 64-битных гостевых ОС исполняемых на 32-битных хостах.</translation>
    </message>
    <message>
        <source>you have 2D Video Acceleration enabled. As 2D Video Acceleration is supported for Windows guests only, this feature will be disabled.</source>
        <translation type="obsolete">для этой машины выбрана функция 2D-ускорения видео. Поскольку данная функция поддерживается лишь классом гостевых систем Windows, она будет отключена.</translation>
    </message>
    <message>
        <source>you have enabled a USB HID (Human Interface Device). This will not work unless USB emulation is also enabled. This will be done automatically when you accept the VM Settings by pressing the OK button.</source>
        <translation type="obsolete">Вы включили поддержку USB HID (устройства пользовательского интерфейса). Данная опция не работает без активированной USB эмуляции, поэтому USB эмуляция будет активирована в момент сохранения настроек виртуальной машины при закрытии данного диалога.</translation>
    </message>
</context>
<context>
    <name>VBoxVMSettingsFD</name>
    <message>
        <source>Host floppy drive is not selected</source>
        <translation type="obsolete">Не выбран физический флоппи-привод</translation>
    </message>
    <message>
        <source>Floppy image file is not selected</source>
        <translation type="obsolete">Не выбран файл образа дискеты</translation>
    </message>
    <message>
        <source>When checked, mounts the specified media to the Floppy drive of the virtual machine.</source>
        <translation type="obsolete">Когда стоит галочка, подключает указанный носитель к приводу гибких дисков виртуальной машины.</translation>
    </message>
    <message>
        <source>&amp;Mount Floppy Drive</source>
        <translation type="obsolete">&amp;Подключить дискету</translation>
    </message>
    <message>
        <source>Mounts the specified host Floppy drive to the virtual Floppy drive.</source>
        <translation type="obsolete">Подключает указанный привод гибких дисков к виртуальному приводу гибких дисков.</translation>
    </message>
    <message>
        <source>Host Floppy &amp;Drive</source>
        <translation type="obsolete">&amp;Физический флоппи-привод</translation>
    </message>
    <message>
        <source>Lists host Floppy drives available to mount to the virtual machine.</source>
        <translation type="obsolete">Показывает список физических приводов гибких дисков, доступных для подключения к виртуальной машине.</translation>
    </message>
    <message>
        <source>Mounts the specified Floppy image to the virtual Floppy drive.</source>
        <translation type="obsolete">Подключает указанный файл образа дискеты к виртуальному приводу гибких дисков.</translation>
    </message>
    <message>
        <source>&amp;Image File</source>
        <translation type="obsolete">Фа&amp;йл образа</translation>
    </message>
    <message>
        <source>Displays the image file to mount to the virtual Floppy drive and allows to quickly select a different image.</source>
        <translation type="obsolete">Показывает файл образа для подключения к виртуальному приводу гибких дисков и позволяет быстро выбрать другой файл образа.</translation>
    </message>
    <message>
        <source>Invokes the Virtual Media Manager to select a Floppy image to mount.</source>
        <translation type="obsolete">Открывает диалог Менеджера виртуальных носителей для выбора файла образа.</translation>
    </message>
</context>
<context>
    <name>VBoxVMSettingsVRDP</name>
    <message>
        <source>When checked, the VM will act as a Remote Desktop Protocol (RDP) server, allowing remote clients to connect and operate the VM (when it is running) using a standard RDP client.</source>
        <translation type="obsolete">Если стоит галочка, то виртуальная машина будет работать как сервер удаленного рабочего стола (RDP), позволяя удаленным клиентам соединяться и использовать ВМ (когда она работает) с помощью стандартного RDP-клиента.</translation>
    </message>
    <message>
        <source>&amp;Enable VRDP Server</source>
        <translation type="obsolete">&amp;Включить VRDP-сервер</translation>
    </message>
    <message>
        <source>Server &amp;Port:</source>
        <translation type="obsolete">&amp;Порт сервера:</translation>
    </message>
    <message>
        <source>Displays the VRDP Server port number. You may specify &lt;tt&gt;0&lt;/tt&gt; (zero) to reset the port to the default value.</source>
        <translation type="obsolete">Показывает номер порта VRDP-сервера. Вы можете указать &lt;tt&gt;0&lt;/tt&gt; (ноль) для сброса номера порта к значению по умолчанию.</translation>
    </message>
    <message>
        <source>Authentication &amp;Method:</source>
        <translation type="obsolete">&amp;Метод аутентификации:</translation>
    </message>
    <message>
        <source>Defines the VRDP authentication method.</source>
        <translation type="obsolete">Задает способ авторизации VRDP-сервера.</translation>
    </message>
    <message>
        <source>Authentication &amp;Timeout:</source>
        <translation type="obsolete">В&amp;ремя ожидания аутентификации:</translation>
    </message>
    <message>
        <source>Specifies the timeout for guest authentication, in milliseconds.</source>
        <translation type="obsolete">Задает максимальное время ожидания авторизации подключения к гостевой ОС в миллисекундах.</translation>
    </message>
</context>
</TS>
