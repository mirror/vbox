; $Id$ 
;; @file
; Auto Generated source file. Do not edit.
;

;
; Source file: bios.c
;
;  
;  Copyright (C) 2006-2012 Oracle Corporation
;  
;  This file is part of VirtualBox Open Source Edition (OSE), as
;  available from http://www.virtualbox.org. This file is free software;
;  you can redistribute it and/or modify it under the terms of the GNU
;  General Public License (GPL) as published by the Free Software
;  Foundation, in version 2 as it comes in the "COPYING" file of the
;  VirtualBox OSE distribution. VirtualBox OSE is distributed in the
;  hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;  --------------------------------------------------------------------
;  
;  This code is based on:
;  
;   ROM BIOS for use with Bochs/Plex86/QEMU emulation environment
;  
;   Copyright (C) 2002  MandrakeSoft S.A.
;  
;     MandrakeSoft S.A.
;     43, rue d'Aboukir
;     75002 Paris - France
;     http://www.linux-mandrake.com/
;     http://www.mandrakesoft.com/
;  
;   This library is free software; you can redistribute it and/or
;   modify it under the terms of the GNU Lesser General Public
;   License as published by the Free Software Foundation; either
;   version 2 of the License, or (at your option) any later version.
;  
;   This library is distributed in the hope that it will be useful,
;   but WITHOUT ANY WARRANTY; without even the implied warranty of
;   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;   Lesser General Public License for more details.
;  
;   You should have received a copy of the GNU Lesser General Public
;   License along with this library; if not, write to the Free Software
;   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
;  

;
; Source file: print.c
;
;  
;  Copyright (C) 2006-2011 Oracle Corporation
;  
;  This file is part of VirtualBox Open Source Edition (OSE), as
;  available from http://www.virtualbox.org. This file is free software;
;  you can redistribute it and/or modify it under the terms of the GNU
;  General Public License (GPL) as published by the Free Software
;  Foundation, in version 2 as it comes in the "COPYING" file of the
;  VirtualBox OSE distribution. VirtualBox OSE is distributed in the
;  hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;  --------------------------------------------------------------------
;  
;  This code is based on:
;  
;   ROM BIOS for use with Bochs/Plex86/QEMU emulation environment
;  
;   Copyright (C) 2002  MandrakeSoft S.A.
;  
;     MandrakeSoft S.A.
;     43, rue d'Aboukir
;     75002 Paris - France
;     http://www.linux-mandrake.com/
;     http://www.mandrakesoft.com/
;  
;   This library is free software; you can redistribute it and/or
;   modify it under the terms of the GNU Lesser General Public
;   License as published by the Free Software Foundation; either
;   version 2 of the License, or (at your option) any later version.
;  
;   This library is distributed in the hope that it will be useful,
;   but WITHOUT ANY WARRANTY; without even the implied warranty of
;   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;   Lesser General Public License for more details.
;  
;   You should have received a copy of the GNU Lesser General Public
;   License along with this library; if not, write to the Free Software
;   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
;  

;
; Source file: ata.c
;
;  
;  Copyright (C) 2006-2012 Oracle Corporation
;  
;  This file is part of VirtualBox Open Source Edition (OSE), as
;  available from http://www.virtualbox.org. This file is free software;
;  you can redistribute it and/or modify it under the terms of the GNU
;  General Public License (GPL) as published by the Free Software
;  Foundation, in version 2 as it comes in the "COPYING" file of the
;  VirtualBox OSE distribution. VirtualBox OSE is distributed in the
;  hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;  --------------------------------------------------------------------
;  
;  This code is based on:
;  
;   ROM BIOS for use with Bochs/Plex86/QEMU emulation environment
;  
;   Copyright (C) 2002  MandrakeSoft S.A.
;  
;     MandrakeSoft S.A.
;     43, rue d'Aboukir
;     75002 Paris - France
;     http://www.linux-mandrake.com/
;     http://www.mandrakesoft.com/
;  
;   This library is free software; you can redistribute it and/or
;   modify it under the terms of the GNU Lesser General Public
;   License as published by the Free Software Foundation; either
;   version 2 of the License, or (at your option) any later version.
;  
;   This library is distributed in the hope that it will be useful,
;   but WITHOUT ANY WARRANTY; without even the implied warranty of
;   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;   Lesser General Public License for more details.
;  
;   You should have received a copy of the GNU Lesser General Public
;   License along with this library; if not, write to the Free Software
;   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
;  

;
; Source file: floppy.c
;
;  
;  Copyright (C) 2006-2012 Oracle Corporation
;  
;  This file is part of VirtualBox Open Source Edition (OSE), as
;  available from http://www.virtualbox.org. This file is free software;
;  you can redistribute it and/or modify it under the terms of the GNU
;  General Public License (GPL) as published by the Free Software
;  Foundation, in version 2 as it comes in the "COPYING" file of the
;  VirtualBox OSE distribution. VirtualBox OSE is distributed in the
;  hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;  --------------------------------------------------------------------
;  
;  This code is based on:
;  
;   ROM BIOS for use with Bochs/Plex86/QEMU emulation environment
;  
;   Copyright (C) 2002  MandrakeSoft S.A.
;  
;     MandrakeSoft S.A.
;     43, rue d'Aboukir
;     75002 Paris - France
;     http://www.linux-mandrake.com/
;     http://www.mandrakesoft.com/
;  
;   This library is free software; you can redistribute it and/or
;   modify it under the terms of the GNU Lesser General Public
;   License as published by the Free Software Foundation; either
;   version 2 of the License, or (at your option) any later version.
;  
;   This library is distributed in the hope that it will be useful,
;   but WITHOUT ANY WARRANTY; without even the implied warranty of
;   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;   Lesser General Public License for more details.
;  
;   You should have received a copy of the GNU Lesser General Public
;   License along with this library; if not, write to the Free Software
;   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
;  

;
; Source file: eltorito.c
;
;  
;  Copyright (C) 2006-2013 Oracle Corporation
;  
;  This file is part of VirtualBox Open Source Edition (OSE), as
;  available from http://www.virtualbox.org. This file is free software;
;  you can redistribute it and/or modify it under the terms of the GNU
;  General Public License (GPL) as published by the Free Software
;  Foundation, in version 2 as it comes in the "COPYING" file of the
;  VirtualBox OSE distribution. VirtualBox OSE is distributed in the
;  hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;  --------------------------------------------------------------------
;  
;  This code is based on:
;  
;   ROM BIOS for use with Bochs/Plex86/QEMU emulation environment
;  
;   Copyright (C) 2002  MandrakeSoft S.A.
;  
;     MandrakeSoft S.A.
;     43, rue d'Aboukir
;     75002 Paris - France
;     http://www.linux-mandrake.com/
;     http://www.mandrakesoft.com/
;  
;   This library is free software; you can redistribute it and/or
;   modify it under the terms of the GNU Lesser General Public
;   License as published by the Free Software Foundation; either
;   version 2 of the License, or (at your option) any later version.
;  
;   This library is distributed in the hope that it will be useful,
;   but WITHOUT ANY WARRANTY; without even the implied warranty of
;   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;   Lesser General Public License for more details.
;  
;   You should have received a copy of the GNU Lesser General Public
;   License along with this library; if not, write to the Free Software
;   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
;  

;
; Source file: boot.c
;
;  
;  Copyright (C) 2006-2012 Oracle Corporation
;  
;  This file is part of VirtualBox Open Source Edition (OSE), as
;  available from http://www.virtualbox.org. This file is free software;
;  you can redistribute it and/or modify it under the terms of the GNU
;  General Public License (GPL) as published by the Free Software
;  Foundation, in version 2 as it comes in the "COPYING" file of the
;  VirtualBox OSE distribution. VirtualBox OSE is distributed in the
;  hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;  --------------------------------------------------------------------
;  
;  This code is based on:
;  
;   ROM BIOS for use with Bochs/Plex86/QEMU emulation environment
;  
;   Copyright (C) 2002  MandrakeSoft S.A.
;  
;     MandrakeSoft S.A.
;     43, rue d'Aboukir
;     75002 Paris - France
;     http://www.linux-mandrake.com/
;     http://www.mandrakesoft.com/
;  
;   This library is free software; you can redistribute it and/or
;   modify it under the terms of the GNU Lesser General Public
;   License as published by the Free Software Foundation; either
;   version 2 of the License, or (at your option) any later version.
;  
;   This library is distributed in the hope that it will be useful,
;   but WITHOUT ANY WARRANTY; without even the implied warranty of
;   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;   Lesser General Public License for more details.
;  
;   You should have received a copy of the GNU Lesser General Public
;   License along with this library; if not, write to the Free Software
;   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
;  

;
; Source file: keyboard.c
;
;  
;  Copyright (C) 2006-2012 Oracle Corporation
;  
;  This file is part of VirtualBox Open Source Edition (OSE), as
;  available from http://www.virtualbox.org. This file is free software;
;  you can redistribute it and/or modify it under the terms of the GNU
;  General Public License (GPL) as published by the Free Software
;  Foundation, in version 2 as it comes in the "COPYING" file of the
;  VirtualBox OSE distribution. VirtualBox OSE is distributed in the
;  hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;  --------------------------------------------------------------------
;  
;  This code is based on:
;  
;   ROM BIOS for use with Bochs/Plex86/QEMU emulation environment
;  
;   Copyright (C) 2002  MandrakeSoft S.A.
;  
;     MandrakeSoft S.A.
;     43, rue d'Aboukir
;     75002 Paris - France
;     http://www.linux-mandrake.com/
;     http://www.mandrakesoft.com/
;  
;   This library is free software; you can redistribute it and/or
;   modify it under the terms of the GNU Lesser General Public
;   License as published by the Free Software Foundation; either
;   version 2 of the License, or (at your option) any later version.
;  
;   This library is distributed in the hope that it will be useful,
;   but WITHOUT ANY WARRANTY; without even the implied warranty of
;   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;   Lesser General Public License for more details.
;  
;   You should have received a copy of the GNU Lesser General Public
;   License along with this library; if not, write to the Free Software
;   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
;  

;
; Source file: disk.c
;
;  
;  Copyright (C) 2006-2012 Oracle Corporation
;  
;  This file is part of VirtualBox Open Source Edition (OSE), as
;  available from http://www.virtualbox.org. This file is free software;
;  you can redistribute it and/or modify it under the terms of the GNU
;  General Public License (GPL) as published by the Free Software
;  Foundation, in version 2 as it comes in the "COPYING" file of the
;  VirtualBox OSE distribution. VirtualBox OSE is distributed in the
;  hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;  --------------------------------------------------------------------
;  
;  This code is based on:
;  
;   ROM BIOS for use with Bochs/Plex86/QEMU emulation environment
;  
;   Copyright (C) 2002  MandrakeSoft S.A.
;  
;     MandrakeSoft S.A.
;     43, rue d'Aboukir
;     75002 Paris - France
;     http://www.linux-mandrake.com/
;     http://www.mandrakesoft.com/
;  
;   This library is free software; you can redistribute it and/or
;   modify it under the terms of the GNU Lesser General Public
;   License as published by the Free Software Foundation; either
;   version 2 of the License, or (at your option) any later version.
;  
;   This library is distributed in the hope that it will be useful,
;   but WITHOUT ANY WARRANTY; without even the implied warranty of
;   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;   Lesser General Public License for more details.
;  
;   You should have received a copy of the GNU Lesser General Public
;   License along with this library; if not, write to the Free Software
;   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
;  

;
; Source file: serial.c
;
;  
;  Copyright (C) 2006-2012 Oracle Corporation
;  
;  This file is part of VirtualBox Open Source Edition (OSE), as
;  available from http://www.virtualbox.org. This file is free software;
;  you can redistribute it and/or modify it under the terms of the GNU
;  General Public License (GPL) as published by the Free Software
;  Foundation, in version 2 as it comes in the "COPYING" file of the
;  VirtualBox OSE distribution. VirtualBox OSE is distributed in the
;  hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;  --------------------------------------------------------------------
;  
;  This code is based on:
;  
;   ROM BIOS for use with Bochs/Plex86/QEMU emulation environment
;  
;   Copyright (C) 2002  MandrakeSoft S.A.
;  
;     MandrakeSoft S.A.
;     43, rue d'Aboukir
;     75002 Paris - France
;     http://www.linux-mandrake.com/
;     http://www.mandrakesoft.com/
;  
;   This library is free software; you can redistribute it and/or
;   modify it under the terms of the GNU Lesser General Public
;   License as published by the Free Software Foundation; either
;   version 2 of the License, or (at your option) any later version.
;  
;   This library is distributed in the hope that it will be useful,
;   but WITHOUT ANY WARRANTY; without even the implied warranty of
;   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;   Lesser General Public License for more details.
;  
;   You should have received a copy of the GNU Lesser General Public
;   License along with this library; if not, write to the Free Software
;   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
;  

;
; Source file: system.c
;
;  
;  Copyright (C) 2006-2013 Oracle Corporation
;  
;  This file is part of VirtualBox Open Source Edition (OSE), as
;  available from http://www.virtualbox.org. This file is free software;
;  you can redistribute it and/or modify it under the terms of the GNU
;  General Public License (GPL) as published by the Free Software
;  Foundation, in version 2 as it comes in the "COPYING" file of the
;  VirtualBox OSE distribution. VirtualBox OSE is distributed in the
;  hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;  --------------------------------------------------------------------
;  
;  This code is based on:
;  
;   ROM BIOS for use with Bochs/Plex86/QEMU emulation environment
;  
;   Copyright (C) 2002  MandrakeSoft S.A.
;  
;     MandrakeSoft S.A.
;     43, rue d'Aboukir
;     75002 Paris - France
;     http://www.linux-mandrake.com/
;     http://www.mandrakesoft.com/
;  
;   This library is free software; you can redistribute it and/or
;   modify it under the terms of the GNU Lesser General Public
;   License as published by the Free Software Foundation; either
;   version 2 of the License, or (at your option) any later version.
;  
;   This library is distributed in the hope that it will be useful,
;   but WITHOUT ANY WARRANTY; without even the implied warranty of
;   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;   Lesser General Public License for more details.
;  
;   You should have received a copy of the GNU Lesser General Public
;   License along with this library; if not, write to the Free Software
;   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
;  

;
; Source file: timepci.c
;
;  
;  Copyright (C) 2006-2012 Oracle Corporation
;  
;  This file is part of VirtualBox Open Source Edition (OSE), as
;  available from http://www.virtualbox.org. This file is free software;
;  you can redistribute it and/or modify it under the terms of the GNU
;  General Public License (GPL) as published by the Free Software
;  Foundation, in version 2 as it comes in the "COPYING" file of the
;  VirtualBox OSE distribution. VirtualBox OSE is distributed in the
;  hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;  --------------------------------------------------------------------
;  
;  This code is based on:
;  
;   ROM BIOS for use with Bochs/Plex86/QEMU emulation environment
;  
;   Copyright (C) 2002  MandrakeSoft S.A.
;  
;     MandrakeSoft S.A.
;     43, rue d'Aboukir
;     75002 Paris - France
;     http://www.linux-mandrake.com/
;     http://www.mandrakesoft.com/
;  
;   This library is free software; you can redistribute it and/or
;   modify it under the terms of the GNU Lesser General Public
;   License as published by the Free Software Foundation; either
;   version 2 of the License, or (at your option) any later version.
;  
;   This library is distributed in the hope that it will be useful,
;   but WITHOUT ANY WARRANTY; without even the implied warranty of
;   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;   Lesser General Public License for more details.
;  
;   You should have received a copy of the GNU Lesser General Public
;   License along with this library; if not, write to the Free Software
;   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
;  

;
; Source file: ps2mouse.c
;
;  
;  Copyright (C) 2006-2011 Oracle Corporation
;  
;  This file is part of VirtualBox Open Source Edition (OSE), as
;  available from http://www.virtualbox.org. This file is free software;
;  you can redistribute it and/or modify it under the terms of the GNU
;  General Public License (GPL) as published by the Free Software
;  Foundation, in version 2 as it comes in the "COPYING" file of the
;  VirtualBox OSE distribution. VirtualBox OSE is distributed in the
;  hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;  --------------------------------------------------------------------
;  
;  This code is based on:
;  
;   ROM BIOS for use with Bochs/Plex86/QEMU emulation environment
;  
;   Copyright (C) 2002  MandrakeSoft S.A.
;  
;     MandrakeSoft S.A.
;     43, rue d'Aboukir
;     75002 Paris - France
;     http://www.linux-mandrake.com/
;     http://www.mandrakesoft.com/
;  
;   This library is free software; you can redistribute it and/or
;   modify it under the terms of the GNU Lesser General Public
;   License as published by the Free Software Foundation; either
;   version 2 of the License, or (at your option) any later version.
;  
;   This library is distributed in the hope that it will be useful,
;   but WITHOUT ANY WARRANTY; without even the implied warranty of
;   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;   Lesser General Public License for more details.
;  
;   You should have received a copy of the GNU Lesser General Public
;   License along with this library; if not, write to the Free Software
;   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
;  

;
; Source file: parallel.c
;
;  
;  Copyright (C) 2006-2012 Oracle Corporation
;  
;  This file is part of VirtualBox Open Source Edition (OSE), as
;  available from http://www.virtualbox.org. This file is free software;
;  you can redistribute it and/or modify it under the terms of the GNU
;  General Public License (GPL) as published by the Free Software
;  Foundation, in version 2 as it comes in the "COPYING" file of the
;  VirtualBox OSE distribution. VirtualBox OSE is distributed in the
;  hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;  --------------------------------------------------------------------
;  
;  This code is based on:
;  
;   ROM BIOS for use with Bochs/Plex86/QEMU emulation environment
;  
;   Copyright (C) 2002  MandrakeSoft S.A.
;  
;     MandrakeSoft S.A.
;     43, rue d'Aboukir
;     75002 Paris - France
;     http://www.linux-mandrake.com/
;     http://www.mandrakesoft.com/
;  
;   This library is free software; you can redistribute it and/or
;   modify it under the terms of the GNU Lesser General Public
;   License as published by the Free Software Foundation; either
;   version 2 of the License, or (at your option) any later version.
;  
;   This library is distributed in the hope that it will be useful,
;   but WITHOUT ANY WARRANTY; without even the implied warranty of
;   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;   Lesser General Public License for more details.
;  
;   You should have received a copy of the GNU Lesser General Public
;   License along with this library; if not, write to the Free Software
;   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
;  

;
; Source file: logo.c
;
;  $Id$
;  Stuff for drawing the BIOS logo.
;  
;  
;  
;  Copyright (C) 2004-2012 Oracle Corporation
;  
;  This file is part of VirtualBox Open Source Edition (OSE), as
;  available from http://www.virtualbox.org. This file is free software;
;  you can redistribute it and/or modify it under the terms of the GNU
;  General Public License (GPL) as published by the Free Software
;  Foundation, in version 2 as it comes in the "COPYING" file of the
;  VirtualBox OSE distribution. VirtualBox OSE is distributed in the
;  hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.

;
; Source file: scsi.c
;
;  $Id$
;  SCSI host adapter driver to boot from SCSI disks
;  
;  
;  
;  Copyright (C) 2004-2012 Oracle Corporation
;  
;  This file is part of VirtualBox Open Source Edition (OSE), as
;  available from http://www.virtualbox.org. This file is free software;
;  you can redistribute it and/or modify it under the terms of the GNU
;  General Public License (GPL) as published by the Free Software
;  Foundation, in version 2 as it comes in the "COPYING" file of the
;  VirtualBox OSE distribution. VirtualBox OSE is distributed in the
;  hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.

;
; Source file: ahci.c
;
;  $Id$
;  AHCI host adapter driver to boot from SATA disks.
;  
;  
;  
;  Copyright (C) 2011-2012 Oracle Corporation
;  
;  This file is part of VirtualBox Open Source Edition (OSE), as
;  available from http://www.virtualbox.org. This file is free software;
;  you can redistribute it and/or modify it under the terms of the GNU
;  General Public License (GPL) as published by the Free Software
;  Foundation, in version 2 as it comes in the "COPYING" file of the
;  VirtualBox OSE distribution. VirtualBox OSE is distributed in the
;  hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.

;
; Source file: apm.c
;
;  $Id$
;  APM BIOS support. Implements APM version 1.2.
;  
;  
;  
;  Copyright (C) 2004-2012 Oracle Corporation
;  
;  This file is part of VirtualBox Open Source Edition (OSE), as
;  available from http://www.virtualbox.org. This file is free software;
;  you can redistribute it and/or modify it under the terms of the GNU
;  General Public License (GPL) as published by the Free Software
;  Foundation, in version 2 as it comes in the "COPYING" file of the
;  VirtualBox OSE distribution. VirtualBox OSE is distributed in the
;  hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.

;
; Source file: pcibios.c
;
;  $Id$
;  PCI BIOS support.
;  
;  
;  
;  Copyright (C) 2004-2012 Oracle Corporation
;  
;  This file is part of VirtualBox Open Source Edition (OSE), as
;  available from http://www.virtualbox.org. This file is free software;
;  you can redistribute it and/or modify it under the terms of the GNU
;  General Public License (GPL) as published by the Free Software
;  Foundation, in version 2 as it comes in the "COPYING" file of the
;  VirtualBox OSE distribution. VirtualBox OSE is distributed in the
;  hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.

;
; Source file: pciutil.c
;
;  Utility routines for calling the PCI BIOS.
;  
;  
;  
;  Copyright (C) 2011 Oracle Corporation
;  
;  This file is part of VirtualBox Open Source Edition (OSE), as
;  available from http://www.virtualbox.org. This file is free software;
;  you can redistribute it and/or modify it under the terms of the GNU
;  General Public License (GPL) as published by the Free Software
;  Foundation, in version 2 as it comes in the "COPYING" file of the
;  VirtualBox OSE distribution. VirtualBox OSE is distributed in the
;  hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.

;
; Source file: vds.c
;
;  Utility routines for calling the Virtual DMA Services.
;  
;  
;  
;  Copyright (C) 2011 Oracle Corporation
;  
;  This file is part of VirtualBox Open Source Edition (OSE), as
;  available from http://www.virtualbox.org. This file is free software;
;  you can redistribute it and/or modify it under the terms of the GNU
;  General Public License (GPL) as published by the Free Software
;  Foundation, in version 2 as it comes in the "COPYING" file of the
;  VirtualBox OSE distribution. VirtualBox OSE is distributed in the
;  hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.

;
; Source file: support.asm
;
;  $Id$
;  Compiler support routines.
;  
;  
;  
;  Copyright (C) 2012 Oracle Corporation
;  
;  This file is part of VirtualBox Open Source Edition (OSE), as
;  available from http://www.virtualbox.org. This file is free software;
;  you can redistribute it and/or modify it under the terms of the GNU
;  General Public License (GPL) as published by the Free Software
;  Foundation, in version 2 as it comes in the "COPYING" file of the
;  VirtualBox OSE distribution. VirtualBox OSE is distributed in the
;  hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;  

;
; Source file: pcibio32.asm
;
;  
;  Copyright (C) 2006-2012 Oracle Corporation
;  
;  This file is part of VirtualBox Open Source Edition (OSE), as
;  available from http://www.virtualbox.org. This file is free software;
;  you can redistribute it and/or modify it under the terms of the GNU
;  General Public License (GPL) as published by the Free Software
;  Foundation, in version 2 as it comes in the "COPYING" file of the
;  VirtualBox OSE distribution. VirtualBox OSE is distributed in the
;  hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;  
;  --------------------------------------------------------------------

;
; Source file: apm_pm.asm
;
;  
;  Copyright (C) 2006-2012 Oracle Corporation
;  
;  This file is part of VirtualBox Open Source Edition (OSE), as
;  available from http://www.virtualbox.org. This file is free software;
;  you can redistribute it and/or modify it under the terms of the GNU
;  General Public License (GPL) as published by the Free Software
;  Foundation, in version 2 as it comes in the "COPYING" file of the
;  VirtualBox OSE distribution. VirtualBox OSE is distributed in the
;  hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;  
;  --------------------------------------------------------------------
;  
;  Protected-mode APM implementation.
;  

;
; Source file: orgs.asm
;
;  
;  Copyright (C) 2006-2011 Oracle Corporation
;  
;  This file is part of VirtualBox Open Source Edition (OSE), as
;  available from http://www.virtualbox.org. This file is free software;
;  you can redistribute it and/or modify it under the terms of the GNU
;  General Public License (GPL) as published by the Free Software
;  Foundation, in version 2 as it comes in the "COPYING" file of the
;  VirtualBox OSE distribution. VirtualBox OSE is distributed in the
;  hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;  --------------------------------------------------------------------
;  
;  This code is based on:
;  
;   ROM BIOS for use with Bochs/Plex86/QEMU emulation environment
;  
;   Copyright (C) 2002  MandrakeSoft S.A.
;  
;     MandrakeSoft S.A.
;     43, rue d'Aboukir
;     75002 Paris - France
;     http://www.linux-mandrake.com/
;     http://www.mandrakesoft.com/
;  
;   This library is free software; you can redistribute it and/or
;   modify it under the terms of the GNU Lesser General Public
;   License as published by the Free Software Foundation; either
;   version 2 of the License, or (at your option) any later version.
;  
;   This library is distributed in the hope that it will be useful,
;   but WITHOUT ANY WARRANTY; without even the implied warranty of
;   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;   Lesser General Public License for more details.
;  
;   You should have received a copy of the GNU Lesser General Public
;   License along with this library; if not, write to the Free Software
;   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
;  
;  

;
; Source file: pci32.c
;
;  $Id$
;  32-bit PCI BIOS wrapper.
;  
;  
;  
;  Copyright (C) 2004-2012 Oracle Corporation
;  
;  This file is part of VirtualBox Open Source Edition (OSE), as
;  available from http://www.virtualbox.org. This file is free software;
;  you can redistribute it and/or modify it under the terms of the GNU
;  General Public License (GPL) as published by the Free Software
;  Foundation, in version 2 as it comes in the "COPYING" file of the
;  VirtualBox OSE distribution. VirtualBox OSE is distributed in the
;  hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.




section _DATA progbits vstart=0x0 align=1 ; size=0x40 class=DATA group=DGROUP
_pktacc:                                     ; 0xf0000 LB 0xc
    db  000h, 000h, 000h, 000h, 000h, 000h, 0eah, 027h, 08dh, 075h, 0adh, 082h
_softrst:                                    ; 0xf000c LB 0xc
    db  000h, 000h, 000h, 000h, 000h, 000h, 0cdh, 02ah, 000h, 000h, 000h, 000h
_dskacc:                                     ; 0xf0018 LB 0x28
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 039h, 027h, 0b0h, 027h, 000h, 000h, 000h, 000h
    db  02bh, 074h, 0dch, 074h, 0afh, 081h, 040h, 082h, 05fh, 033h, 032h, 05fh, 000h, 0dah, 00fh, 000h
    db  000h, 001h, 0f3h, 000h, 000h, 000h, 000h, 000h

section CONST progbits vstart=0x40 align=1 ; size=0xce0 class=DATA group=DGROUP
    db   'NMI Handler called', 00ah, 000h
    db   'INT18: BOOT FAILURE', 00ah, 000h
    db   '%s', 00ah, 000h, 000h
    db   'FATAL: ', 000h
    db   'bios_printf: unknown format', 00ah, 000h, 000h
    db   'ata-detect: Failed to detect ATA device', 00ah, 000h
    db   'ata%d-%d: PCHS=%u/%d/%d LCHS=%u/%u/%u', 00ah, 000h
    db   'ata-detect: Failed to detect ATAPI device', 00ah, 000h
    db   ' slave', 000h
    db   'master', 000h
    db   'ata%d %s: ', 000h
    db   '%c', 000h
    db   ' ATA-%d Hard-Disk (%lu MBytes)', 00ah, 000h
    db   ' ATAPI-%d CD-ROM/DVD-ROM', 00ah, 000h
    db   ' ATAPI-%d Device', 00ah, 000h
    db   'ata%d %s: Unknown device', 00ah, 000h
    db   'ata_cmd_packet', 000h
    db   '%s: DATA_OUT not supported yet', 00ah, 000h
    db   'set_diskette_current_cyl: drive > 1', 00ah, 000h
    db   'int13_diskette_function', 000h
    db   '%s: drive>1 || head>1 ...', 00ah, 000h
    db   '%s: ctrl not ready', 00ah, 000h
    db   '%s: write error', 00ah, 000h
    db   '%s: bad floppy type', 00ah, 000h
    db   '%s: unsupported AH=%02x', 00ah, 000h, 000h
    db   'int13_eltorito', 000h
    db   '%s: call with AX=%04x not implemented.', 00ah, 000h
    db   '%s: unsupported AH=%02x', 00ah, 000h
    db   'int13_cdemu', 000h
    db   '%s: function %02x, emulation not active for DL= %02x', 00ah, 000h
    db   '%s: function %02x, error %02x !', 00ah, 000h
    db   '%s: function AH=%02x unsupported, returns fail', 00ah, 000h
    db   'int13_cdrom', 000h
    db   '%s: function %02x, ELDL out of range %02x', 00ah, 000h
    db   '%s: function %02x, unmapped device for ELDL=%02x', 00ah, 000h
    db   '%s: function %02x. Can', 027h, 't use 64bits lba', 00ah, 000h
    db   '%s: function %02x, status %02x !', 00ah, 000h, 000h
    db   'Booting from %s...', 00ah, 000h
    db   'Boot from %s failed', 00ah, 000h
    db   'Boot from %s %d failed', 00ah, 000h
    db   'No bootable medium found! System halted.', 00ah, 000h
    db   'Could not read from the boot medium! System halted.', 00ah, 000h
    db   'CDROM boot failure code : %04x', 00ah, 000h
    db   'Boot : bseqnr=%d, bootseq=%x', 00dh, 00ah, 000h, 000h
    db   'Keyboard error:%u', 00ah, 000h
    db   'KBD: int09 handler: AL=0', 00ah, 000h
    db   'KBD: int09h_handler(): unknown scancode read: 0x%02x!', 00ah, 000h
    db   'KBD: int09h_handler(): scancode & asciicode are zero?', 00ah, 000h
    db   'KBD: int16h: out of keyboard input', 00ah, 000h
    db   'KBD: unsupported int 16h function %02x', 00ah, 000h
    db   'AX=%04x BX=%04x CX=%04x DX=%04x ', 00ah, 000h, 000h
    db   'int13_harddisk', 000h
    db   '%s: function %02x, ELDL out of range %02x', 00ah, 000h
    db   '%s: function %02x, unmapped device for ELDL=%02x', 00ah, 000h
    db   '%s: function %02x, count out of range!', 00ah, 000h
    db   '%s: function %02x, disk %02x, parameters out of range %04x/%04x/%04x!', 00ah
    db   000h
    db   '%s: function %02x, error %02x !', 00ah, 000h
    db   'format disk track called', 00ah, 000h
    db   '%s: function %02xh unimplemented, returns success', 00ah, 000h
    db   '%s: function %02xh unsupported, returns fail', 00ah, 000h
    db   'int13_harddisk_ext', 000h
    db   '%s: function %02x. Can', 027h, 't use 64bits lba', 00ah, 000h
    db   '%s: function %02x. LBA out of range', 00ah, 000h
    db   'int15: Func 24h, subfunc %02xh, A20 gate control not supported', 00ah, 000h
    db   '*** int 15h function AH=bf not yet supported!', 00ah, 000h
    db   'EISA BIOS not present', 00ah, 000h
    db   '*** int 15h function AX=%04x, BX=%04x not yet supported!', 00ah, 000h
    db   'sendmouse', 000h
    db   'setkbdcomm', 000h
    db   'Mouse reset returned %02x (should be ack)', 00ah, 000h
    db   'Mouse status returned %02x (should be ack)', 00ah, 000h
    db   'INT 15h C2 AL=6, BH=%02x', 00ah, 000h
    db   'INT 15h C2 default case entered', 00ah, 000h, 000h
    db   'Key pressed: %x', 00ah, 000h
    db   00ah, 00ah, 'AHCI controller:', 00ah, 000h
    db   00ah, '    %d) Hard disk', 000h
    db   00ah, 00ah, 'SCSI controller:', 00ah, 000h
    db   'IDE controller:', 00ah, 000h
    db   00ah, '    %d) ', 000h
    db   'Secondary ', 000h
    db   'Primary ', 000h
    db   'Slave', 000h
    db   'Master', 000h
    db   'No hard disks found', 000h
    db   00ah, 000h
    db   'Press F12 to select boot device.', 00ah, 000h
    db   00ah, 'VirtualBox temporary boot device selection', 00ah, 00ah, 'Detected H'
    db   'ard disks:', 00ah, 00ah, 000h
    db   00ah, 'Other boot devices:', 00ah, ' f) Floppy', 00ah, ' c) CD-ROM', 00ah
    db   ' l) LAN', 00ah, 00ah, ' b) Continue booting', 00ah, 000h
    db   'Delaying boot for %d seconds:', 000h
    db   ' %d', 000h
    db   'scsi_read_sectors: device_id out of range %d', 00ah, 000h
    db   'scsi_write_sectors: device_id out of range %d', 00ah, 000h
    db   'scsi_cmd_packet', 000h
    db   '%s: DATA_OUT not supported yet', 00ah, 000h
    db   'scsi_enumerate_attached_devices', 000h
    db   '%s: SCSI_INQUIRY failed', 00ah, 000h
    db   '%s: SCSI_READ_CAPACITY failed', 00ah, 000h
    db   'Disk %d has an unsupported sector size of %u', 00ah, 000h
    db   'SCSI %d-ID#%d: LCHS=%u/%u/%u %ld sectors', 00ah, 000h
    db   'SCSI %d-ID#%d: CD/DVD-ROM', 00ah, 000h
    db   'ahci_read_sectors', 000h
    db   '%s: device_id out of range %d', 00ah, 000h
    db   'ahci_write_sectors', 000h
    db   'ahci_cmd_packet', 000h
    db   '%s: DATA_OUT not supported yet', 00ah, 000h
    db   'AHCI %d-P#%d: PCHS=%u/%d/%d LCHS=%u/%u/%u %ld sectors', 00ah, 000h, 000h
    db   'Standby', 000h
    db   'Suspend', 000h
    db   'Shutdown', 000h
    db   'APM: Unsupported function AX=%04X BX=%04X called', 00ah, 000h, 000h
    db   'PCI: Unsupported function AX=%04X BX=%04X called', 00ah, 000h

section CONST2 progbits vstart=0xd20 align=1 ; size=0x3fa class=DATA group=DGROUP
_bios_cvs_version_string:                    ; 0xf0d20 LB 0x12
    db  'VirtualBox 4.2.51', 000h
_bios_prefix_string:                         ; 0xf0d32 LB 0x8
    db  'BIOS: ', 000h, 000h
_isotag:                                     ; 0xf0d3a LB 0x6
    db  'CD001', 000h
_eltorito:                                   ; 0xf0d40 LB 0x18
    db  'EL TORITO SPECIFICATION', 000h
_drivetypes:                                 ; 0xf0d58 LB 0x28
    db  046h, 06ch, 06fh, 070h, 070h, 079h, 000h, 000h, 000h, 000h, 048h, 061h, 072h, 064h, 020h, 044h
    db  069h, 073h, 06bh, 000h, 043h, 044h, 02dh, 052h, 04fh, 04dh, 000h, 000h, 000h, 000h, 04ch, 041h
    db  04eh, 000h, 000h, 000h, 000h, 000h, 000h, 000h
_scan_to_scanascii:                          ; 0xf0d80 LB 0x37a
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 01bh, 001h, 01bh, 001h, 01bh, 001h
    db  000h, 001h, 000h, 000h, 031h, 002h, 021h, 002h, 000h, 000h, 000h, 078h, 000h, 000h, 032h, 003h
    db  040h, 003h, 000h, 003h, 000h, 079h, 000h, 000h, 033h, 004h, 023h, 004h, 000h, 000h, 000h, 07ah
    db  000h, 000h, 034h, 005h, 024h, 005h, 000h, 000h, 000h, 07bh, 000h, 000h, 035h, 006h, 025h, 006h
    db  000h, 000h, 000h, 07ch, 000h, 000h, 036h, 007h, 05eh, 007h, 01eh, 007h, 000h, 07dh, 000h, 000h
    db  037h, 008h, 026h, 008h, 000h, 000h, 000h, 07eh, 000h, 000h, 038h, 009h, 02ah, 009h, 000h, 000h
    db  000h, 07fh, 000h, 000h, 039h, 00ah, 028h, 00ah, 000h, 000h, 000h, 080h, 000h, 000h, 030h, 00bh
    db  029h, 00bh, 000h, 000h, 000h, 081h, 000h, 000h, 02dh, 00ch, 05fh, 00ch, 01fh, 00ch, 000h, 082h
    db  000h, 000h, 03dh, 00dh, 02bh, 00dh, 000h, 000h, 000h, 083h, 000h, 000h, 008h, 00eh, 008h, 00eh
    db  07fh, 00eh, 000h, 000h, 000h, 000h, 009h, 00fh, 000h, 00fh, 000h, 000h, 000h, 000h, 000h, 000h
    db  071h, 010h, 051h, 010h, 011h, 010h, 000h, 010h, 040h, 000h, 077h, 011h, 057h, 011h, 017h, 011h
    db  000h, 011h, 040h, 000h, 065h, 012h, 045h, 012h, 005h, 012h, 000h, 012h, 040h, 000h, 072h, 013h
    db  052h, 013h, 012h, 013h, 000h, 013h, 040h, 000h, 074h, 014h, 054h, 014h, 014h, 014h, 000h, 014h
    db  040h, 000h, 079h, 015h, 059h, 015h, 019h, 015h, 000h, 015h, 040h, 000h, 075h, 016h, 055h, 016h
    db  015h, 016h, 000h, 016h, 040h, 000h, 069h, 017h, 049h, 017h, 009h, 017h, 000h, 017h, 040h, 000h
    db  06fh, 018h, 04fh, 018h, 00fh, 018h, 000h, 018h, 040h, 000h, 070h, 019h, 050h, 019h, 010h, 019h
    db  000h, 019h, 040h, 000h, 05bh, 01ah, 07bh, 01ah, 01bh, 01ah, 000h, 000h, 000h, 000h, 05dh, 01bh
    db  07dh, 01bh, 01dh, 01bh, 000h, 000h, 000h, 000h, 00dh, 01ch, 00dh, 01ch, 00ah, 01ch, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 061h, 01eh, 041h, 01eh
    db  001h, 01eh, 000h, 01eh, 040h, 000h, 073h, 01fh, 053h, 01fh, 013h, 01fh, 000h, 01fh, 040h, 000h
    db  064h, 020h, 044h, 020h, 004h, 020h, 000h, 020h, 040h, 000h, 066h, 021h, 046h, 021h, 006h, 021h
    db  000h, 021h, 040h, 000h, 067h, 022h, 047h, 022h, 007h, 022h, 000h, 022h, 040h, 000h, 068h, 023h
    db  048h, 023h, 008h, 023h, 000h, 023h, 040h, 000h, 06ah, 024h, 04ah, 024h, 00ah, 024h, 000h, 024h
    db  040h, 000h, 06bh, 025h, 04bh, 025h, 00bh, 025h, 000h, 025h, 040h, 000h, 06ch, 026h, 04ch, 026h
    db  00ch, 026h, 000h, 026h, 040h, 000h, 03bh, 027h, 03ah, 027h, 000h, 000h, 000h, 000h, 000h, 000h
    db  027h, 028h, 022h, 028h, 000h, 000h, 000h, 000h, 000h, 000h, 060h, 029h, 07eh, 029h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 05ch, 02bh
    db  07ch, 02bh, 01ch, 02bh, 000h, 000h, 000h, 000h, 07ah, 02ch, 05ah, 02ch, 01ah, 02ch, 000h, 02ch
    db  040h, 000h, 078h, 02dh, 058h, 02dh, 018h, 02dh, 000h, 02dh, 040h, 000h, 063h, 02eh, 043h, 02eh
    db  003h, 02eh, 000h, 02eh, 040h, 000h, 076h, 02fh, 056h, 02fh, 016h, 02fh, 000h, 02fh, 040h, 000h
    db  062h, 030h, 042h, 030h, 002h, 030h, 000h, 030h, 040h, 000h, 06eh, 031h, 04eh, 031h, 00eh, 031h
    db  000h, 031h, 040h, 000h, 06dh, 032h, 04dh, 032h, 00dh, 032h, 000h, 032h, 040h, 000h, 02ch, 033h
    db  03ch, 033h, 000h, 000h, 000h, 000h, 000h, 000h, 02eh, 034h, 03eh, 034h, 000h, 000h, 000h, 000h
    db  000h, 000h, 02fh, 035h, 03fh, 035h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 02ah, 037h, 02ah, 037h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 020h, 039h, 020h, 039h, 020h, 039h
    db  020h, 039h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 03bh
    db  000h, 054h, 000h, 05eh, 000h, 068h, 000h, 000h, 000h, 03ch, 000h, 055h, 000h, 05fh, 000h, 069h
    db  000h, 000h, 000h, 03dh, 000h, 056h, 000h, 060h, 000h, 06ah, 000h, 000h, 000h, 03eh, 000h, 057h
    db  000h, 061h, 000h, 06bh, 000h, 000h, 000h, 03fh, 000h, 058h, 000h, 062h, 000h, 06ch, 000h, 000h
    db  000h, 040h, 000h, 059h, 000h, 063h, 000h, 06dh, 000h, 000h, 000h, 041h, 000h, 05ah, 000h, 064h
    db  000h, 06eh, 000h, 000h, 000h, 042h, 000h, 05bh, 000h, 065h, 000h, 06fh, 000h, 000h, 000h, 043h
    db  000h, 05ch, 000h, 066h, 000h, 070h, 000h, 000h, 000h, 044h, 000h, 05dh, 000h, 067h, 000h, 071h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 047h, 037h, 047h, 000h, 077h, 000h, 000h, 020h, 000h
    db  000h, 048h, 038h, 048h, 000h, 000h, 000h, 000h, 020h, 000h, 000h, 049h, 039h, 049h, 000h, 084h
    db  000h, 000h, 020h, 000h, 02dh, 04ah, 02dh, 04ah, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 04bh
    db  034h, 04bh, 000h, 073h, 000h, 000h, 020h, 000h, 000h, 04ch, 035h, 04ch, 000h, 000h, 000h, 000h
    db  020h, 000h, 000h, 04dh, 036h, 04dh, 000h, 074h, 000h, 000h, 020h, 000h, 02bh, 04eh, 02bh, 04eh
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 04fh, 031h, 04fh, 000h, 075h, 000h, 000h, 020h, 000h
    db  000h, 050h, 032h, 050h, 000h, 000h, 000h, 000h, 020h, 000h, 000h, 051h, 033h, 051h, 000h, 076h
    db  000h, 000h, 020h, 000h, 000h, 052h, 030h, 052h, 000h, 000h, 000h, 000h, 020h, 000h, 000h, 053h
    db  02eh, 053h, 000h, 000h, 000h, 000h, 020h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 05ch, 056h, 07ch, 056h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 085h, 000h, 087h, 000h, 089h, 000h, 08bh, 000h, 000h
    db  000h, 086h, 000h, 088h, 000h, 08ah, 000h, 08ch, 000h, 000h
_panic_msg_keyb_buffer_full:                 ; 0xf10fa LB 0x20
    db  '%s: keyboard input buffer full', 00ah, 000h

  ; Padding 0x4e6 bytes at 0xf111a
  times 1254 db 0

section _TEXT progbits vstart=0x1600 align=1 ; size=0x7aa5 class=CODE group=AUTO
read_byte_:                                  ; 0xf1600 LB 0xe
    push bx                                   ; 53
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, dx                                ; 89 d3
    mov es, ax                                ; 8e c0
    mov al, byte [es:bx]                      ; 26 8a 07
    pop bp                                    ; 5d
    pop bx                                    ; 5b
    retn                                      ; c3
write_byte_:                                 ; 0xf160e LB 0xe
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov si, dx                                ; 89 d6
    mov es, ax                                ; 8e c0
    mov byte [es:si], bl                      ; 26 88 1c
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn                                      ; c3
read_word_:                                  ; 0xf161c LB 0xe
    push bx                                   ; 53
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, dx                                ; 89 d3
    mov es, ax                                ; 8e c0
    mov ax, word [es:bx]                      ; 26 8b 07
    pop bp                                    ; 5d
    pop bx                                    ; 5b
    retn                                      ; c3
write_word_:                                 ; 0xf162a LB 0xe
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov si, dx                                ; 89 d6
    mov es, ax                                ; 8e c0
    mov word [es:si], bx                      ; 26 89 1c
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn                                      ; c3
read_dword_:                                 ; 0xf1638 LB 0x12
    push bx                                   ; 53
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, dx                                ; 89 d3
    mov es, ax                                ; 8e c0
    mov ax, word [es:bx]                      ; 26 8b 07
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02
    pop bp                                    ; 5d
    pop bx                                    ; 5b
    retn                                      ; c3
write_dword_:                                ; 0xf164a LB 0x12
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov si, dx                                ; 89 d6
    mov es, ax                                ; 8e c0
    mov word [es:si], bx                      ; 26 89 1c
    mov word [es:si+002h], cx                 ; 26 89 4c 02
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn                                      ; c3
inb_cmos_:                                   ; 0xf165c LB 0x11
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, strict word 00070h                ; ba 70 00
    out DX, AL                                ; ee
    mov dx, strict word 00071h                ; ba 71 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    retn                                      ; c3
outb_cmos_:                                  ; 0xf166d LB 0x11
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov ah, dl                                ; 88 d4
    mov dx, strict word 00070h                ; ba 70 00
    out DX, AL                                ; ee
    mov al, ah                                ; 88 e0
    mov dx, strict word 00071h                ; ba 71 00
    out DX, AL                                ; ee
    pop bp                                    ; 5d
    retn                                      ; c3
_dummy_isr_function:                         ; 0xf167e LB 0x69
    enter 00002h, 000h                        ; c8 02 00 00
    mov CL, strict byte 0ffh                  ; b1 ff
    mov AL, strict byte 00bh                  ; b0 0b
    mov dx, strict word 00020h                ; ba 20 00
    out DX, AL                                ; ee
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov bx, ax                                ; 89 c3
    mov byte [bp-002h], al                    ; 88 46 fe
    test al, al                               ; 84 c0
    je short 016d9h                           ; 74 43
    mov AL, strict byte 00bh                  ; b0 0b
    mov dx, 000a0h                            ; ba a0 00
    out DX, AL                                ; ee
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov cx, ax                                ; 89 c1
    test al, al                               ; 84 c0
    je short 016bbh                           ; 74 16
    mov dx, 000a1h                            ; ba a1 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    xor ah, ah                                ; 30 e4
    movzx bx, cl                              ; 0f b6 d9
    or ax, bx                                 ; 09 d8
    out DX, AL                                ; ee
    mov AL, strict byte 020h                  ; b0 20
    mov dx, 000a0h                            ; ba a0 00
    out DX, AL                                ; ee
    jmp short 016d0h                          ; eb 15
    mov dx, strict word 00021h                ; ba 21 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and bl, 0fbh                              ; 80 e3 fb
    mov byte [bp-002h], bl                    ; 88 5e fe
    xor ah, ah                                ; 30 e4
    movzx bx, byte [bp-002h]                  ; 0f b6 5e fe
    or ax, bx                                 ; 09 d8
    out DX, AL                                ; ee
    mov AL, strict byte 020h                  ; b0 20
    mov dx, strict word 00020h                ; ba 20 00
    out DX, AL                                ; ee
    mov cl, byte [bp-002h]                    ; 8a 4e fe
    movzx bx, cl                              ; 0f b6 d9
    mov dx, strict word 0006bh                ; ba 6b 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 29 ff
    leave                                     ; c9
    retn                                      ; c3
_nmi_handler_msg:                            ; 0xf16e7 LB 0x10
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push strict word 00040h                   ; 68 40 00
    push strict byte 00007h                   ; 6a 07
    call 018e9h                               ; e8 f7 01
    add sp, strict byte 00004h                ; 83 c4 04
    pop bp                                    ; 5d
    retn                                      ; c3
_int18_panic_msg:                            ; 0xf16f7 LB 0x10
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push strict word 00054h                   ; 68 54 00
    push strict byte 00007h                   ; 6a 07
    call 018e9h                               ; e8 e7 01
    add sp, strict byte 00004h                ; 83 c4 04
    pop bp                                    ; 5d
    retn                                      ; c3
_log_bios_start:                             ; 0xf1707 LB 0x1e
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 99 01
    push 00d20h                               ; 68 20 0d
    push strict word 00069h                   ; 68 69 00
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 c9 01
    add sp, strict byte 00006h                ; 83 c4 06
    pop bp                                    ; 5d
    retn                                      ; c3
_print_bios_banner:                          ; 0xf1725 LB 0x2c
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, strict word 00072h                ; ba 72 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 eb fe
    mov cx, ax                                ; 89 c1
    xor bx, bx                                ; 31 db
    mov dx, strict word 00072h                ; ba 72 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0162ah                               ; e8 ec fe
    cmp cx, 01234h                            ; 81 f9 34 12
    jne short 0174ch                          ; 75 08
    mov AL, strict byte 003h                  ; b0 03
    mov AH, strict byte 000h                  ; b4 00
    int 010h                                  ; cd 10
    pop bp                                    ; 5d
    retn                                      ; c3
    call 07097h                               ; e8 48 59
    pop bp                                    ; 5d
    retn                                      ; c3
send_:                                       ; 0xf1751 LB 0x38
    push bx                                   ; 53
    push cx                                   ; 51
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, ax                                ; 89 c3
    mov cl, dl                                ; 88 d1
    test AL, strict byte 008h                 ; a8 08
    je short 01764h                           ; 74 06
    mov al, dl                                ; 88 d0
    mov dx, 00403h                            ; ba 03 04
    out DX, AL                                ; ee
    test bl, 004h                             ; f6 c3 04
    je short 0176fh                           ; 74 06
    mov al, cl                                ; 88 c8
    mov dx, 00504h                            ; ba 04 05
    out DX, AL                                ; ee
    test bl, 002h                             ; f6 c3 02
    je short 01785h                           ; 74 11
    cmp cl, 00ah                              ; 80 f9 0a
    jne short 0177fh                          ; 75 06
    mov AL, strict byte 00dh                  ; b0 0d
    mov AH, strict byte 00eh                  ; b4 0e
    int 010h                                  ; cd 10
    mov al, cl                                ; 88 c8
    mov AH, strict byte 00eh                  ; b4 0e
    int 010h                                  ; cd 10
    pop bp                                    ; 5d
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
put_int_:                                    ; 0xf1789 LB 0x5b
    push si                                   ; 56
    push di                                   ; 57
    enter 00004h, 000h                        ; c8 04 00 00
    mov si, ax                                ; 89 c6
    mov word [bp-004h], dx                    ; 89 56 fc
    mov di, strict word 0000ah                ; bf 0a 00
    mov ax, dx                                ; 89 d0
    cwd                                       ; 99
    idiv di                                   ; f7 ff
    mov word [bp-002h], ax                    ; 89 46 fe
    test ax, ax                               ; 85 c0
    je short 017adh                           ; 74 0a
    dec bx                                    ; 4b
    mov dx, ax                                ; 89 c2
    mov ax, si                                ; 89 f0
    call 01789h                               ; e8 de ff
    jmp short 017c8h                          ; eb 1b
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jle short 017bch                          ; 7e 0a
    mov dx, strict word 00020h                ; ba 20 00
    mov ax, si                                ; 89 f0
    call 01751h                               ; e8 97 ff
    jmp short 017adh                          ; eb f1
    test cx, cx                               ; 85 c9
    je short 017c8h                           ; 74 08
    mov dx, strict word 0002dh                ; ba 2d 00
    mov ax, si                                ; 89 f0
    call 01751h                               ; e8 89 ff
    mov al, byte [bp-002h]                    ; 8a 46 fe
    mov DL, strict byte 00ah                  ; b2 0a
    mul dl                                    ; f6 e2
    mov dl, byte [bp-004h]                    ; 8a 56 fc
    sub dl, al                                ; 28 c2
    mov al, dl                                ; 88 d0
    add AL, strict byte 030h                  ; 04 30
    movzx dx, al                              ; 0f b6 d0
    mov ax, si                                ; 89 f0
    call 01751h                               ; e8 71 ff
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
put_uint_:                                   ; 0xf17e4 LB 0x5c
    push si                                   ; 56
    push di                                   ; 57
    enter 00004h, 000h                        ; c8 04 00 00
    mov si, ax                                ; 89 c6
    mov word [bp-004h], dx                    ; 89 56 fc
    mov ax, dx                                ; 89 d0
    xor dx, dx                                ; 31 d2
    mov di, strict word 0000ah                ; bf 0a 00
    div di                                    ; f7 f7
    mov word [bp-002h], ax                    ; 89 46 fe
    test ax, ax                               ; 85 c0
    je short 01809h                           ; 74 0a
    dec bx                                    ; 4b
    mov dx, ax                                ; 89 c2
    mov ax, si                                ; 89 f0
    call 017e4h                               ; e8 dd ff
    jmp short 01824h                          ; eb 1b
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jle short 01818h                          ; 7e 0a
    mov dx, strict word 00020h                ; ba 20 00
    mov ax, si                                ; 89 f0
    call 01751h                               ; e8 3b ff
    jmp short 01809h                          ; eb f1
    test cx, cx                               ; 85 c9
    je short 01824h                           ; 74 08
    mov dx, strict word 0002dh                ; ba 2d 00
    mov ax, si                                ; 89 f0
    call 01751h                               ; e8 2d ff
    mov al, byte [bp-002h]                    ; 8a 46 fe
    mov DL, strict byte 00ah                  ; b2 0a
    mul dl                                    ; f6 e2
    mov dl, byte [bp-004h]                    ; 8a 56 fc
    sub dl, al                                ; 28 c2
    mov al, dl                                ; 88 d0
    add AL, strict byte 030h                  ; 04 30
    movzx dx, al                              ; 0f b6 d0
    mov ax, si                                ; 89 f0
    call 01751h                               ; e8 15 ff
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
put_luint_:                                  ; 0xf1840 LB 0x6e
    push si                                   ; 56
    push di                                   ; 57
    enter 00004h, 000h                        ; c8 04 00 00
    mov si, ax                                ; 89 c6
    mov word [bp-002h], bx                    ; 89 5e fe
    mov di, dx                                ; 89 d7
    mov ax, bx                                ; 89 d8
    mov dx, cx                                ; 89 ca
    mov bx, strict word 0000ah                ; bb 0a 00
    xor cx, cx                                ; 31 c9
    call 08fb0h                               ; e8 57 77
    mov word [bp-004h], ax                    ; 89 46 fc
    mov cx, dx                                ; 89 d1
    mov dx, ax                                ; 89 c2
    or dx, cx                                 ; 09 ca
    je short 01873h                           ; 74 0f
    push word [bp+008h]                       ; ff 76 08
    lea dx, [di-001h]                         ; 8d 55 ff
    mov bx, ax                                ; 89 c3
    mov ax, si                                ; 89 f0
    call 01840h                               ; e8 cf ff
    jmp short 01890h                          ; eb 1d
    dec di                                    ; 4f
    test di, di                               ; 85 ff
    jle short 01882h                          ; 7e 0a
    mov dx, strict word 00020h                ; ba 20 00
    mov ax, si                                ; 89 f0
    call 01751h                               ; e8 d1 fe
    jmp short 01873h                          ; eb f1
    cmp word [bp+008h], strict byte 00000h    ; 83 7e 08 00
    je short 01890h                           ; 74 08
    mov dx, strict word 0002dh                ; ba 2d 00
    mov ax, si                                ; 89 f0
    call 01751h                               ; e8 c1 fe
    mov al, byte [bp-004h]                    ; 8a 46 fc
    mov DL, strict byte 00ah                  ; b2 0a
    mul dl                                    ; f6 e2
    mov dl, byte [bp-002h]                    ; 8a 56 fe
    sub dl, al                                ; 28 c2
    mov al, dl                                ; 88 d0
    add AL, strict byte 030h                  ; 04 30
    movzx dx, al                              ; 0f b6 d0
    mov ax, si                                ; 89 f0
    call 01751h                               ; e8 a9 fe
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn 00002h                               ; c2 02 00
put_str_:                                    ; 0xf18ae LB 0x1e
    push dx                                   ; 52
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov si, ax                                ; 89 c6
    mov es, cx                                ; 8e c1
    mov dl, byte [es:bx]                      ; 26 8a 17
    test dl, dl                               ; 84 d2
    je short 018c8h                           ; 74 0a
    xor dh, dh                                ; 30 f6
    mov ax, si                                ; 89 f0
    call 01751h                               ; e8 8c fe
    inc bx                                    ; 43
    jmp short 018b5h                          ; eb ed
    pop bp                                    ; 5d
    pop si                                    ; 5e
    pop dx                                    ; 5a
    retn                                      ; c3
put_str_near_:                               ; 0xf18cc LB 0x1d
    push bx                                   ; 53
    push cx                                   ; 51
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov cx, ax                                ; 89 c1
    mov bx, dx                                ; 89 d3
    mov dl, byte [bx]                         ; 8a 17
    test dl, dl                               ; 84 d2
    je short 018e5h                           ; 74 0a
    xor dh, dh                                ; 30 f6
    mov ax, cx                                ; 89 c8
    call 01751h                               ; e8 6f fe
    inc bx                                    ; 43
    jmp short 018d5h                          ; eb f0
    pop bp                                    ; 5d
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
bios_printf_:                                ; 0xf18e9 LB 0x23c
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push si                                   ; 56
    push di                                   ; 57
    enter 00008h, 000h                        ; c8 08 00 00
    lea bx, [bp+012h]                         ; 8d 5e 12
    mov word [bp-008h], bx                    ; 89 5e f8
    mov [bp-006h], ss                         ; 8c 56 fa
    xor cx, cx                                ; 31 c9
    xor si, si                                ; 31 f6
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    and ax, strict word 00007h                ; 25 07 00
    cmp ax, strict word 00007h                ; 3d 07 00
    jne short 0191bh                          ; 75 11
    xor al, al                                ; 30 c0
    mov dx, 00401h                            ; ba 01 04
    out DX, AL                                ; ee
    push strict word 0006eh                   ; 68 6e 00
    push strict byte 00002h                   ; 6a 02
    call 018e9h                               ; e8 d1 ff
    add sp, strict byte 00004h                ; 83 c4 04
    mov bx, word [bp+010h]                    ; 8b 5e 10
    mov dl, byte [bx]                         ; 8a 17
    test dl, dl                               ; 84 d2
    je near 01b0ch                            ; 0f 84 e6 01
    cmp dl, 025h                              ; 80 fa 25
    jne short 01933h                          ; 75 08
    mov cx, strict word 00001h                ; b9 01 00
    xor si, si                                ; 31 f6
    jmp near 01b06h                           ; e9 d3 01
    test cx, cx                               ; 85 c9
    je near 01afeh                            ; 0f 84 c5 01
    cmp dl, 030h                              ; 80 fa 30
    jc short 01951h                           ; 72 13
    cmp dl, 039h                              ; 80 fa 39
    jnbe short 01951h                         ; 77 0e
    movzx ax, dl                              ; 0f b6 c2
    imul si, si, strict byte 0000ah           ; 6b f6 0a
    sub ax, strict word 00030h                ; 2d 30 00
    add si, ax                                ; 01 c6
    jmp near 01b06h                           ; e9 b5 01
    mov ax, word [bp-006h]                    ; 8b 46 fa
    mov word [bp-006h], ax                    ; 89 46 fa
    add word [bp-008h], strict byte 00002h    ; 83 46 f8 02
    les bx, [bp-008h]                         ; c4 5e f8
    mov ax, word [es:bx-002h]                 ; 26 8b 47 fe
    mov word [bp-002h], ax                    ; 89 46 fe
    cmp dl, 078h                              ; 80 fa 78
    je short 0196fh                           ; 74 05
    cmp dl, 058h                              ; 80 fa 58
    jne short 019b8h                          ; 75 49
    test si, si                               ; 85 f6
    jne short 01976h                          ; 75 03
    mov si, strict word 00004h                ; be 04 00
    cmp dl, 078h                              ; 80 fa 78
    jne short 01980h                          ; 75 05
    mov di, strict word 00061h                ; bf 61 00
    jmp short 01983h                          ; eb 03
    mov di, strict word 00041h                ; bf 41 00
    lea bx, [si-001h]                         ; 8d 5c ff
    test bx, bx                               ; 85 db
    jl near 01afah                            ; 0f 8c 6e 01
    mov cx, bx                                ; 89 d9
    sal cx, 002h                              ; c1 e1 02
    mov ax, word [bp-002h]                    ; 8b 46 fe
    shr ax, CL                                ; d3 e8
    xor ah, ah                                ; 30 e4
    and AL, strict byte 00fh                  ; 24 0f
    cmp ax, strict word 00009h                ; 3d 09 00
    jnbe short 019a6h                         ; 77 07
    mov dx, ax                                ; 89 c2
    add dx, strict byte 00030h                ; 83 c2 30
    jmp short 019adh                          ; eb 07
    mov dx, ax                                ; 89 c2
    sub dx, strict byte 0000ah                ; 83 ea 0a
    add dx, di                                ; 01 fa
    xor dh, dh                                ; 30 f6
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    call 01751h                               ; e8 9c fd
    dec bx                                    ; 4b
    jmp short 01986h                          ; eb ce
    cmp dl, 075h                              ; 80 fa 75
    jne short 019cch                          ; 75 0f
    xor cx, cx                                ; 31 c9
    mov bx, si                                ; 89 f3
    mov dx, ax                                ; 89 c2
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    call 017e4h                               ; e8 1b fe
    jmp near 01afah                           ; e9 2e 01
    lea bx, [si-001h]                         ; 8d 5c ff
    cmp dl, 06ch                              ; 80 fa 6c
    jne near 01a88h                           ; 0f 85 b2 00
    inc word [bp+010h]                        ; ff 46 10
    mov di, word [bp+010h]                    ; 8b 7e 10
    mov dl, byte [di]                         ; 8a 15
    mov ax, word [bp-006h]                    ; 8b 46 fa
    mov word [bp-006h], ax                    ; 89 46 fa
    add word [bp-008h], strict byte 00002h    ; 83 46 f8 02
    les di, [bp-008h]                         ; c4 7e f8
    mov ax, word [es:di-002h]                 ; 26 8b 45 fe
    mov word [bp-004h], ax                    ; 89 46 fc
    cmp dl, 064h                              ; 80 fa 64
    jne short 01a24h                          ; 75 2d
    test byte [bp-003h], 080h                 ; f6 46 fd 80
    je short 01a12h                           ; 74 15
    push strict byte 00001h                   ; 6a 01
    mov ax, word [bp-002h]                    ; 8b 46 fe
    mov cx, word [bp-004h]                    ; 8b 4e fc
    neg cx                                    ; f7 d9
    neg ax                                    ; f7 d8
    sbb cx, strict byte 00000h                ; 83 d9 00
    mov dx, bx                                ; 89 da
    mov bx, ax                                ; 89 c3
    jmp short 01a1bh                          ; eb 09
    push strict byte 00000h                   ; 6a 00
    mov bx, word [bp-002h]                    ; 8b 5e fe
    mov dx, si                                ; 89 f2
    mov cx, ax                                ; 89 c1
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    call 01840h                               ; e8 1f fe
    jmp near 01afah                           ; e9 d6 00
    cmp dl, 075h                              ; 80 fa 75
    jne short 01a2bh                          ; 75 02
    jmp short 01a12h                          ; eb e7
    cmp dl, 078h                              ; 80 fa 78
    je short 01a37h                           ; 74 07
    cmp dl, 058h                              ; 80 fa 58
    jne near 01afah                           ; 0f 85 c3 00
    test si, si                               ; 85 f6
    jne short 01a3eh                          ; 75 03
    mov si, strict word 00008h                ; be 08 00
    cmp dl, 078h                              ; 80 fa 78
    jne short 01a48h                          ; 75 05
    mov di, strict word 00061h                ; bf 61 00
    jmp short 01a4bh                          ; eb 03
    mov di, strict word 00041h                ; bf 41 00
    lea bx, [si-001h]                         ; 8d 5c ff
    test bx, bx                               ; 85 db
    jl near 01afah                            ; 0f 8c a6 00
    mov ax, word [bp-002h]                    ; 8b 46 fe
    mov cx, bx                                ; 89 d9
    sal cx, 002h                              ; c1 e1 02
    mov dx, word [bp-004h]                    ; 8b 56 fc
    jcxz 01a67h                               ; e3 06
    shr dx, 1                                 ; d1 ea
    rcr ax, 1                                 ; d1 d8
    loop 01a61h                               ; e2 fa
    and ax, strict word 0000fh                ; 25 0f 00
    cmp ax, strict word 00009h                ; 3d 09 00
    jnbe short 01a76h                         ; 77 07
    mov dx, ax                                ; 89 c2
    add dx, strict byte 00030h                ; 83 c2 30
    jmp short 01a7dh                          ; eb 07
    mov dx, ax                                ; 89 c2
    sub dx, strict byte 0000ah                ; 83 ea 0a
    add dx, di                                ; 01 fa
    xor dh, dh                                ; 30 f6
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    call 01751h                               ; e8 cc fc
    dec bx                                    ; 4b
    jmp short 01a4eh                          ; eb c6
    cmp dl, 064h                              ; 80 fa 64
    jne short 01aaah                          ; 75 1d
    test byte [bp-001h], 080h                 ; f6 46 ff 80
    je short 01a9ch                           ; 74 09
    mov dx, ax                                ; 89 c2
    neg dx                                    ; f7 da
    mov cx, strict word 00001h                ; b9 01 00
    jmp short 01aa2h                          ; eb 06
    xor cx, cx                                ; 31 c9
    mov bx, si                                ; 89 f3
    mov dx, ax                                ; 89 c2
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    call 01789h                               ; e8 e1 fc
    jmp short 01afah                          ; eb 50
    cmp dl, 073h                              ; 80 fa 73
    jne short 01abbh                          ; 75 0c
    mov cx, ds                                ; 8c d9
    mov bx, ax                                ; 89 c3
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    call 018aeh                               ; e8 f5 fd
    jmp short 01afah                          ; eb 3f
    cmp dl, 053h                              ; 80 fa 53
    jne short 01adeh                          ; 75 1e
    mov word [bp-004h], ax                    ; 89 46 fc
    mov ax, word [bp-006h]                    ; 8b 46 fa
    mov word [bp-006h], ax                    ; 89 46 fa
    add word [bp-008h], strict byte 00002h    ; 83 46 f8 02
    les bx, [bp-008h]                         ; c4 5e f8
    mov ax, word [es:bx-002h]                 ; 26 8b 47 fe
    mov word [bp-002h], ax                    ; 89 46 fe
    mov bx, ax                                ; 89 c3
    mov cx, word [bp-004h]                    ; 8b 4e fc
    jmp short 01ab3h                          ; eb d5
    cmp dl, 063h                              ; 80 fa 63
    jne short 01aefh                          ; 75 0c
    movzx dx, byte [bp-002h]                  ; 0f b6 56 fe
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    call 01751h                               ; e8 64 fc
    jmp short 01afah                          ; eb 0b
    push strict word 00076h                   ; 68 76 00
    push strict byte 00007h                   ; 6a 07
    call 018e9h                               ; e8 f2 fd
    add sp, strict byte 00004h                ; 83 c4 04
    xor cx, cx                                ; 31 c9
    jmp short 01b06h                          ; eb 08
    xor dh, dh                                ; 30 f6
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    call 01751h                               ; e8 4b fc
    inc word [bp+010h]                        ; ff 46 10
    jmp near 0191bh                           ; e9 0f fe
    xor ax, ax                                ; 31 c0
    mov word [bp-008h], ax                    ; 89 46 f8
    mov word [bp-006h], ax                    ; 89 46 fa
    test byte [bp+00eh], 001h                 ; f6 46 0e 01
    je short 01b1eh                           ; 74 04
    cli                                       ; fa
    hlt                                       ; f4
    jmp short 01b1bh                          ; eb fd
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
_ata_init:                                   ; 0xf1b25 LB 0xc1
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 ea fa
    mov si, 00122h                            ; be 22 01
    mov dx, ax                                ; 89 c2
    xor al, al                                ; 30 c0
    jmp short 01b3fh                          ; eb 04
    cmp AL, strict byte 004h                  ; 3c 04
    jnc short 01b63h                          ; 73 24
    movzx bx, al                              ; 0f b6 d8
    imul bx, bx, strict byte 00006h           ; 6b db 06
    mov es, dx                                ; 8e c2
    add bx, si                                ; 01 f3
    mov byte [es:bx+001c0h], 000h             ; 26 c6 87 c0 01 00
    db  066h, 026h, 0c7h, 087h, 0c2h, 001h, 000h, 000h, 000h, 000h
    ; mov dword [es:bx+001c2h], strict dword 000000000h ; 66 26 c7 87 c2 01 00 00 00 00
    mov byte [es:bx+001c1h], 000h             ; 26 c6 87 c1 01 00
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    jmp short 01b3bh                          ; eb d8
    xor al, al                                ; 30 c0
    jmp short 01b6bh                          ; eb 04
    cmp AL, strict byte 008h                  ; 3c 08
    jnc short 01bb6h                          ; 73 4b
    movzx bx, al                              ; 0f b6 d8
    imul bx, bx, strict byte 00018h           ; 6b db 18
    mov es, dx                                ; 8e c2
    add bx, si                                ; 01 f3
    db  066h, 026h, 0c7h, 047h, 01eh, 000h, 000h, 000h, 000h
    ; mov dword [es:bx+01eh], strict dword 000000000h ; 66 26 c7 47 1e 00 00 00 00
    mov byte [es:bx+022h], 000h               ; 26 c6 47 22 00
    mov word [es:bx+024h], 00200h             ; 26 c7 47 24 00 02
    mov byte [es:bx+023h], 000h               ; 26 c6 47 23 00
    db  066h, 026h, 0c7h, 047h, 026h, 000h, 000h, 000h, 000h
    ; mov dword [es:bx+026h], strict dword 000000000h ; 66 26 c7 47 26 00 00 00 00
    db  066h, 026h, 0c7h, 047h, 02ah, 000h, 000h, 000h, 000h
    ; mov dword [es:bx+02ah], strict dword 000000000h ; 66 26 c7 47 2a 00 00 00 00
    db  066h, 026h, 0c7h, 047h, 02eh, 000h, 000h, 000h, 000h
    ; mov dword [es:bx+02eh], strict dword 000000000h ; 66 26 c7 47 2e 00 00 00 00
    db  066h, 026h, 0c7h, 047h, 032h, 000h, 000h, 000h, 000h
    ; mov dword [es:bx+032h], strict dword 000000000h ; 66 26 c7 47 32 00 00 00 00
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    jmp short 01b67h                          ; eb b1
    xor al, al                                ; 30 c0
    jmp short 01bbeh                          ; eb 04
    cmp AL, strict byte 010h                  ; 3c 10
    jnc short 01bd5h                          ; 73 17
    movzx bx, al                              ; 0f b6 d8
    mov es, dx                                ; 8e c2
    add bx, si                                ; 01 f3
    mov byte [es:bx+0019fh], 010h             ; 26 c6 87 9f 01 10
    mov byte [es:bx+001b0h], 010h             ; 26 c6 87 b0 01 10
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    jmp short 01bbah                          ; eb e5
    mov es, dx                                ; 8e c2
    mov byte [es:si+0019eh], 000h             ; 26 c6 84 9e 01 00
    mov byte [es:si+001afh], 000h             ; 26 c6 84 af 01 00
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn                                      ; c3
ata_reset_:                                  ; 0xf1be6 LB 0xda
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push si                                   ; 56
    push di                                   ; 57
    enter 00004h, 000h                        ; c8 04 00 00
    push ax                                   ; 50
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 23 fa
    mov word [bp-004h], 00122h                ; c7 46 fc 22 01
    mov di, ax                                ; 89 c7
    mov bx, word [bp-006h]                    ; 8b 5e fa
    shr bx, 1                                 ; d1 eb
    mov dl, byte [bp-006h]                    ; 8a 56 fa
    and dl, 001h                              ; 80 e2 01
    mov byte [bp-002h], dl                    ; 88 56 fe
    xor bh, bh                                ; 30 ff
    imul bx, bx, strict byte 00006h           ; 6b db 06
    mov es, ax                                ; 8e c0
    add bx, 00122h                            ; 81 c3 22 01
    mov cx, word [es:bx+001c2h]               ; 26 8b 8f c2 01
    mov si, word [es:bx+001c4h]               ; 26 8b b7 c4 01
    lea dx, [si+006h]                         ; 8d 54 06
    mov AL, strict byte 00eh                  ; b0 0e
    out DX, AL                                ; ee
    mov bx, 000ffh                            ; bb ff 00
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 01c3dh                          ; 76 0c
    mov dx, cx                                ; 89 ca
    add dx, strict byte 00007h                ; 83 c2 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 080h                 ; a8 80
    je short 01c2ch                           ; 74 ef
    lea dx, [si+006h]                         ; 8d 54 06
    mov AL, strict byte 00ah                  ; b0 0a
    out DX, AL                                ; ee
    imul bx, word [bp-006h], strict byte 00018h ; 6b 5e fa 18
    mov es, di                                ; 8e c7
    add bx, word [bp-004h]                    ; 03 5e fc
    cmp byte [es:bx+01eh], 000h               ; 26 80 7f 1e 00
    je short 01c9fh                           ; 74 4c
    cmp byte [bp-002h], 000h                  ; 80 7e fe 00
    je short 01c5eh                           ; 74 05
    mov ax, 000b0h                            ; b8 b0 00
    jmp short 01c61h                          ; eb 03
    mov ax, 000a0h                            ; b8 a0 00
    mov dx, cx                                ; 89 ca
    add dx, strict byte 00006h                ; 83 c2 06
    out DX, AL                                ; ee
    mov dx, cx                                ; 89 ca
    inc dx                                    ; 42
    inc dx                                    ; 42
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov bx, ax                                ; 89 c3
    mov dx, cx                                ; 89 ca
    add dx, strict byte 00003h                ; 83 c2 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp bl, 001h                              ; 80 fb 01
    jne short 01c9fh                          ; 75 22
    cmp al, bl                                ; 38 d8
    jne short 01c9fh                          ; 75 1e
    mov bx, strict word 0ffffh                ; bb ff ff
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 01c9fh                          ; 76 16
    mov dx, cx                                ; 89 ca
    add dx, strict byte 00007h                ; 83 c2 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 080h                 ; a8 80
    je short 01c9fh                           ; 74 0a
    mov ax, strict word 0ffffh                ; b8 ff ff
    dec ax                                    ; 48
    test ax, ax                               ; 85 c0
    jnbe short 01c98h                         ; 77 fb
    jmp short 01c84h                          ; eb e5
    mov bx, strict word 00010h                ; bb 10 00
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 01cb3h                          ; 76 0c
    mov dx, cx                                ; 89 ca
    add dx, strict byte 00007h                ; 83 c2 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 040h                 ; a8 40
    je short 01ca2h                           ; 74 ef
    lea dx, [si+006h]                         ; 8d 54 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
ata_cmd_data_in_:                            ; 0xf1cc0 LB 0x253
    push si                                   ; 56
    push di                                   ; 57
    enter 0001ch, 000h                        ; c8 1c 00 00
    mov si, ax                                ; 89 c6
    mov word [bp-006h], dx                    ; 89 56 fa
    mov word [bp-012h], bx                    ; 89 5e ee
    mov word [bp-010h], cx                    ; 89 4e f0
    mov es, dx                                ; 8e c2
    movzx ax, byte [es:si+008h]               ; 26 0f b6 44 08
    mov dx, ax                                ; 89 c2
    shr dx, 1                                 ; d1 ea
    mov dh, al                                ; 88 c6
    and dh, 001h                              ; 80 e6 01
    mov byte [bp-004h], dh                    ; 88 76 fc
    movzx di, dl                              ; 0f b6 fa
    imul di, di, strict byte 00006h           ; 6b ff 06
    add di, si                                ; 01 f7
    mov bx, word [es:di+001c2h]               ; 26 8b 9d c2 01
    mov dx, word [es:di+001c4h]               ; 26 8b 95 c4 01
    mov word [bp-018h], dx                    ; 89 56 e8
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    mov di, si                                ; 89 f7
    add di, ax                                ; 01 c7
    mov al, byte [es:di+022h]                 ; 26 8a 45 22
    mov byte [bp-002h], al                    ; 88 46 fe
    mov ax, word [es:di+024h]                 ; 26 8b 45 24
    mov word [bp-008h], ax                    ; 89 46 f8
    test ax, ax                               ; 85 c0
    jne short 01d26h                          ; 75 14
    cmp byte [bp-002h], 001h                  ; 80 7e fe 01
    jne short 01d1fh                          ; 75 07
    mov word [bp-008h], 04000h                ; c7 46 f8 00 40
    jmp short 01d35h                          ; eb 16
    mov word [bp-008h], 08000h                ; c7 46 f8 00 80
    jmp short 01d35h                          ; eb 0f
    cmp byte [bp-002h], 001h                  ; 80 7e fe 01
    jne short 01d32h                          ; 75 06
    shr word [bp-008h], 002h                  ; c1 6e f8 02
    jmp short 01d35h                          ; eb 03
    shr word [bp-008h], 1                     ; d1 6e f8
    lea dx, [bx+007h]                         ; 8d 57 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 080h                 ; a8 80
    je short 01d4eh                           ; 74 0f
    mov dx, word [bp-018h]                    ; 8b 56 e8
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00001h                ; ba 01 00
    jmp near 01f0dh                           ; e9 bf 01
    mov es, [bp-006h]                         ; 8e 46 fa
    mov ax, word [es:si]                      ; 26 8b 04
    mov word [bp-01ch], ax                    ; 89 46 e4
    mov ax, word [es:si+002h]                 ; 26 8b 44 02
    mov word [bp-01ah], ax                    ; 89 46 e6
    mov di, word [es:si+004h]                 ; 26 8b 7c 04
    mov ax, word [es:si+006h]                 ; 26 8b 44 06
    mov word [bp-00ah], ax                    ; 89 46 f6
    mov ax, word [es:si+012h]                 ; 26 8b 44 12
    mov word [bp-00ch], ax                    ; 89 46 f4
    mov ax, word [es:si+00eh]                 ; 26 8b 44 0e
    mov word [bp-00eh], ax                    ; 89 46 f2
    mov ax, word [es:si+010h]                 ; 26 8b 44 10
    mov word [bp-014h], ax                    ; 89 46 ec
    mov ax, word [bp-00ch]                    ; 8b 46 f4
    test ax, ax                               ; 85 c0
    jne short 01dech                          ; 75 67
    mov dx, word [bp-01ch]                    ; 8b 56 e4
    add dx, word [bp-010h]                    ; 03 56 f0
    adc ax, word [bp-01ah]                    ; 13 46 e6
    cmp ax, 01000h                            ; 3d 00 10
    jnbe short 01d95h                         ; 77 02
    jne short 01dc0h                          ; 75 2b
    mov ax, word [bp-01ah]                    ; 8b 46 e6
    xor al, al                                ; 30 c0
    shr ax, 008h                              ; c1 e8 08
    mov word [bp-00ch], ax                    ; 89 46 f4
    mov ax, word [bp-010h]                    ; 8b 46 f0
    xor al, al                                ; 30 c0
    shr ax, 008h                              ; c1 e8 08
    lea dx, [bx+002h]                         ; 8d 57 02
    out DX, AL                                ; ee
    lea dx, [bx+003h]                         ; 8d 57 03
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    out DX, AL                                ; ee
    lea dx, [bx+004h]                         ; 8d 57 04
    xor al, al                                ; 30 c0
    out DX, AL                                ; ee
    lea dx, [bx+005h]                         ; 8d 57 05
    out DX, AL                                ; ee
    mov byte [bp-019h], al                    ; 88 46 e7
    mov ax, word [bp-01ch]                    ; 8b 46 e4
    xor ah, ah                                ; 30 e4
    mov word [bp-00ch], ax                    ; 89 46 f4
    mov cx, strict word 00008h                ; b9 08 00
    shr word [bp-01ah], 1                     ; d1 6e e6
    rcr word [bp-01ch], 1                     ; d1 5e e4
    loop 01dcbh                               ; e2 f8
    mov ax, word [bp-01ch]                    ; 8b 46 e4
    mov word [bp-00eh], ax                    ; 89 46 f2
    mov ax, word [bp-01ah]                    ; 8b 46 e6
    mov word [bp-01ch], ax                    ; 89 46 e4
    mov word [bp-01ah], strict word 00000h    ; c7 46 e6 00 00
    and ax, strict word 0000fh                ; 25 0f 00
    or AL, strict byte 040h                   ; 0c 40
    mov word [bp-014h], ax                    ; 89 46 ec
    mov dx, word [bp-018h]                    ; 8b 56 e8
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 00ah                  ; b0 0a
    out DX, AL                                ; ee
    lea dx, [bx+001h]                         ; 8d 57 01
    xor al, al                                ; 30 c0
    out DX, AL                                ; ee
    lea dx, [bx+002h]                         ; 8d 57 02
    mov al, byte [bp-010h]                    ; 8a 46 f0
    out DX, AL                                ; ee
    lea dx, [bx+003h]                         ; 8d 57 03
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    out DX, AL                                ; ee
    mov ax, word [bp-00eh]                    ; 8b 46 f2
    lea dx, [bx+004h]                         ; 8d 57 04
    out DX, AL                                ; ee
    shr ax, 008h                              ; c1 e8 08
    lea dx, [bx+005h]                         ; 8d 57 05
    out DX, AL                                ; ee
    cmp byte [bp-004h], 000h                  ; 80 7e fc 00
    je short 01e22h                           ; 74 05
    mov ax, 000b0h                            ; b8 b0 00
    jmp short 01e25h                          ; eb 03
    mov ax, 000a0h                            ; b8 a0 00
    movzx dx, byte [bp-014h]                  ; 0f b6 56 ec
    or ax, dx                                 ; 09 d0
    lea dx, [bx+006h]                         ; 8d 57 06
    out DX, AL                                ; ee
    lea dx, [bx+007h]                         ; 8d 57 07
    mov al, byte [bp-012h]                    ; 8a 46 ee
    out DX, AL                                ; ee
    mov ax, word [bp-012h]                    ; 8b 46 ee
    cmp ax, 000c4h                            ; 3d c4 00
    je short 01e43h                           ; 74 05
    cmp ax, strict word 00029h                ; 3d 29 00
    jne short 01e50h                          ; 75 0d
    mov ax, word [bp-010h]                    ; 8b 46 f0
    mov word [bp-016h], ax                    ; 89 46 ea
    mov word [bp-010h], strict word 00001h    ; c7 46 f0 01 00
    jmp short 01e55h                          ; eb 05
    mov word [bp-016h], strict word 00001h    ; c7 46 ea 01 00
    lea dx, [bx+007h]                         ; 8d 57 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov dl, al                                ; 88 c2
    test AL, strict byte 080h                 ; a8 80
    jne short 01e55h                          ; 75 f4
    test AL, strict byte 001h                 ; a8 01
    je short 01e74h                           ; 74 0f
    mov dx, word [bp-018h]                    ; 8b 56 e8
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00002h                ; ba 02 00
    jmp near 01f0dh                           ; e9 99 00
    test dl, 008h                             ; f6 c2 08
    jne short 01e88h                          ; 75 0f
    mov dx, word [bp-018h]                    ; 8b 56 e8
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00003h                ; ba 03 00
    jmp near 01f0dh                           ; e9 85 00
    sti                                       ; fb
    cmp di, 0f800h                            ; 81 ff 00 f8
    jc short 01e9ch                           ; 72 0d
    sub di, 00800h                            ; 81 ef 00 08
    mov ax, word [bp-00ah]                    ; 8b 46 f6
    add ax, 00080h                            ; 05 80 00
    mov word [bp-00ah], ax                    ; 89 46 f6
    cmp byte [bp-002h], 001h                  ; 80 7e fe 01
    jne short 01eafh                          ; 75 0d
    mov dx, bx                                ; 89 da
    mov cx, word [bp-008h]                    ; 8b 4e f8
    mov es, [bp-00ah]                         ; 8e 46 f6
    db  0f3h, 066h, 06dh
    ; rep insd                                  ; f3 66 6d
    jmp short 01eb9h                          ; eb 0a
    mov dx, bx                                ; 89 da
    mov cx, word [bp-008h]                    ; 8b 4e f8
    mov es, [bp-00ah]                         ; 8e 46 f6
    rep insw                                  ; f3 6d
    mov ax, word [bp-016h]                    ; 8b 46 ea
    mov es, [bp-006h]                         ; 8e 46 fa
    add word [es:si+014h], ax                 ; 26 01 44 14
    dec word [bp-010h]                        ; ff 4e f0
    lea dx, [bx+007h]                         ; 8d 57 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov dl, al                                ; 88 c2
    test AL, strict byte 080h                 ; a8 80
    jne short 01ec6h                          ; 75 f4
    cmp word [bp-010h], strict byte 00000h    ; 83 7e f0 00
    jne short 01eech                          ; 75 14
    and AL, strict byte 0c9h                  ; 24 c9
    cmp AL, strict byte 040h                  ; 3c 40
    je short 01f02h                           ; 74 24
    mov dx, word [bp-018h]                    ; 8b 56 e8
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00004h                ; ba 04 00
    jmp short 01f0dh                          ; eb 21
    mov al, dl                                ; 88 d0
    and AL, strict byte 0c9h                  ; 24 c9
    cmp AL, strict byte 048h                  ; 3c 48
    je short 01e89h                           ; 74 95
    mov dx, word [bp-018h]                    ; 8b 56 e8
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00005h                ; ba 05 00
    jmp short 01f0dh                          ; eb 0b
    mov dx, word [bp-018h]                    ; 8b 56 e8
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    xor dx, dx                                ; 31 d2
    mov ax, dx                                ; 89 d0
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
_ata_detect:                                 ; 0xf1f13 LB 0x611
    push si                                   ; 56
    push di                                   ; 57
    enter 0025ch, 000h                        ; c8 5c 02 00
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 fa f6
    mov word [bp-020h], ax                    ; 89 46 e0
    mov di, 00122h                            ; bf 22 01
    mov es, ax                                ; 8e c0
    mov word [bp-024h], di                    ; 89 7e dc
    mov word [bp-022h], ax                    ; 89 46 de
    mov byte [es:di+001c0h], 000h             ; 26 c6 85 c0 01 00
    db  066h, 026h, 0c7h, 085h, 0c2h, 001h, 0f0h, 001h, 0f0h, 003h
    ; mov dword [es:di+001c2h], strict dword 003f001f0h ; 66 26 c7 85 c2 01 f0 01 f0 03
    mov byte [es:di+001c1h], 00eh             ; 26 c6 85 c1 01 0e
    mov byte [es:di+001c6h], 000h             ; 26 c6 85 c6 01 00
    db  066h, 026h, 0c7h, 085h, 0c8h, 001h, 070h, 001h, 070h, 003h
    ; mov dword [es:di+001c8h], strict dword 003700170h ; 66 26 c7 85 c8 01 70 01 70 03
    mov byte [es:di+001c7h], 00fh             ; 26 c6 85 c7 01 0f
    xor al, al                                ; 30 c0
    mov byte [bp-006h], al                    ; 88 46 fa
    mov byte [bp-002h], al                    ; 88 46 fe
    mov byte [bp-00ah], al                    ; 88 46 f6
    jmp near 024b2h                           ; e9 48 05
    mov ax, 000a0h                            ; b8 a0 00
    lea dx, [si+006h]                         ; 8d 54 06
    out DX, AL                                ; ee
    lea di, [si+002h]                         ; 8d 7c 02
    mov AL, strict byte 055h                  ; b0 55
    mov dx, di                                ; 89 fa
    out DX, AL                                ; ee
    lea bx, [si+003h]                         ; 8d 5c 03
    mov AL, strict byte 0aah                  ; b0 aa
    mov dx, bx                                ; 89 da
    out DX, AL                                ; ee
    mov dx, di                                ; 89 fa
    out DX, AL                                ; ee
    mov AL, strict byte 055h                  ; b0 55
    mov dx, bx                                ; 89 da
    out DX, AL                                ; ee
    mov dx, di                                ; 89 fa
    out DX, AL                                ; ee
    mov AL, strict byte 0aah                  ; b0 aa
    mov dx, bx                                ; 89 da
    out DX, AL                                ; ee
    mov dx, di                                ; 89 fa
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov cx, ax                                ; 89 c1
    mov dx, bx                                ; 89 da
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp cl, 055h                              ; 80 f9 55
    jne near 02064h                           ; 0f 85 c0 00
    cmp AL, strict byte 0aah                  ; 3c aa
    jne near 02064h                           ; 0f 85 ba 00
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    les bx, [bp-024h]                         ; c4 5e dc
    add bx, ax                                ; 01 c3
    mov byte [es:bx+01eh], 001h               ; 26 c6 47 1e 01
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6
    call 01be6h                               ; e8 24 fc
    cmp byte [bp-004h], 000h                  ; 80 7e fc 00
    je short 01fcdh                           ; 74 05
    mov ax, 000b0h                            ; b8 b0 00
    jmp short 01fd0h                          ; eb 03
    mov ax, 000a0h                            ; b8 a0 00
    lea dx, [si+006h]                         ; 8d 54 06
    out DX, AL                                ; ee
    lea dx, [si+002h]                         ; 8d 54 02
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov bx, ax                                ; 89 c3
    lea dx, [si+003h]                         ; 8d 54 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp bl, 001h                              ; 80 fb 01
    jne near 02064h                           ; 0f 85 7b 00
    cmp al, bl                                ; 38 d8
    jne near 02064h                           ; 0f 85 75 00
    lea dx, [si+004h]                         ; 8d 54 04
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov bx, ax                                ; 89 c3
    mov bh, al                                ; 88 c7
    lea dx, [si+005h]                         ; 8d 54 05
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov cx, ax                                ; 89 c1
    mov byte [bp-008h], al                    ; 88 46 f8
    lea dx, [si+007h]                         ; 8d 54 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp bl, 014h                              ; 80 fb 14
    jne short 02027h                          ; 75 18
    cmp cl, 0ebh                              ; 80 f9 eb
    jne short 02027h                          ; 75 13
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    les bx, [bp-024h]                         ; c4 5e dc
    add bx, ax                                ; 01 c3
    mov byte [es:bx+01eh], 003h               ; 26 c6 47 1e 03
    jmp short 02064h                          ; eb 3d
    test bh, bh                               ; 84 ff
    jne short 02049h                          ; 75 1e
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00
    jne short 02049h                          ; 75 18
    test al, al                               ; 84 c0
    je short 02049h                           ; 74 14
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6
    imul bx, bx, strict byte 00018h           ; 6b db 18
    mov es, [bp-022h]                         ; 8e 46 de
    add bx, word [bp-024h]                    ; 03 5e dc
    mov byte [es:bx+01eh], 002h               ; 26 c6 47 1e 02
    jmp short 02064h                          ; eb 1b
    cmp bh, 0ffh                              ; 80 ff ff
    jne short 02064h                          ; 75 16
    cmp bh, byte [bp-008h]                    ; 3a 7e f8
    jne short 02064h                          ; 75 11
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    les bx, [bp-024h]                         ; c4 5e dc
    add bx, ax                                ; 01 c3
    mov byte [es:bx+01eh], 000h               ; 26 c6 47 1e 00
    mov dx, word [bp-028h]                    ; 8b 56 d8
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    movzx si, byte [bp-00ah]                  ; 0f b6 76 f6
    imul si, si, strict byte 00018h           ; 6b f6 18
    mov es, [bp-022h]                         ; 8e 46 de
    add si, word [bp-024h]                    ; 03 76 dc
    mov al, byte [es:si+01eh]                 ; 26 8a 44 1e
    mov byte [bp-00ch], al                    ; 88 46 f4
    cmp AL, strict byte 002h                  ; 3c 02
    jne near 0228ah                           ; 0f 85 03 02
    mov byte [es:si+01fh], 0ffh               ; 26 c6 44 1f ff
    mov byte [es:si+022h], 000h               ; 26 c6 44 22 00
    lea dx, [bp-0025ch]                       ; 8d 96 a4 fd
    mov bx, word [bp-024h]                    ; 8b 5e dc
    mov word [es:bx+004h], dx                 ; 26 89 57 04
    mov [es:bx+006h], ss                      ; 26 8c 57 06
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    mov byte [es:bx+008h], al                 ; 26 88 47 08
    mov cx, strict word 00001h                ; b9 01 00
    mov bx, 000ech                            ; bb ec 00
    mov ax, word [bp-024h]                    ; 8b 46 dc
    mov dx, es                                ; 8c c2
    call 01cc0h                               ; e8 0b fc
    test ax, ax                               ; 85 c0
    je short 020c4h                           ; 74 0b
    push 00094h                               ; 68 94 00
    push strict byte 00007h                   ; 6a 07
    call 018e9h                               ; e8 28 f8
    add sp, strict byte 00004h                ; 83 c4 04
    test byte [bp-0025ch], 080h               ; f6 86 a4 fd 80
    db  00fh, 095h, 0c0h
    ; setne al                                  ; 0f 95 c0
    xor ah, ah                                ; 30 e4
    mov byte [bp-010h], al                    ; 88 46 f0
    cmp byte [bp-001fch], 000h                ; 80 be 04 fe 00
    db  00fh, 095h, 0c0h
    ; setne al                                  ; 0f 95 c0
    xor ah, ah                                ; 30 e4
    mov byte [bp-012h], al                    ; 88 46 ee
    mov word [bp-01eh], 00200h                ; c7 46 e2 00 02
    mov ax, word [bp-0025ah]                  ; 8b 86 a6 fd
    mov word [bp-016h], ax                    ; 89 46 ea
    mov ax, word [bp-00256h]                  ; 8b 86 aa fd
    mov word [bp-018h], ax                    ; 89 46 e8
    mov ax, word [bp-00250h]                  ; 8b 86 b0 fd
    mov word [bp-01ah], ax                    ; 89 46 e6
    mov ax, word [bp-001e4h]                  ; 8b 86 1c fe
    mov word [bp-01ch], ax                    ; 89 46 e4
    mov si, word [bp-001e2h]                  ; 8b b6 1e fe
    cmp si, 00fffh                            ; 81 fe ff 0f
    jne short 02119h                          ; 75 10
    cmp ax, strict word 0ffffh                ; 3d ff ff
    jne short 02119h                          ; 75 0b
    mov ax, word [bp-00194h]                  ; 8b 86 6c fe
    mov word [bp-01ch], ax                    ; 89 46 e4
    mov si, word [bp-00192h]                  ; 8b b6 6e fe
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    cmp AL, strict byte 001h                  ; 3c 01
    jc short 0212ch                           ; 72 0c
    jbe short 02134h                          ; 76 12
    cmp AL, strict byte 003h                  ; 3c 03
    je short 0213ch                           ; 74 16
    cmp AL, strict byte 002h                  ; 3c 02
    je short 02138h                           ; 74 0e
    jmp short 02179h                          ; eb 4d
    test al, al                               ; 84 c0
    jne short 02179h                          ; 75 49
    mov BL, strict byte 01eh                  ; b3 1e
    jmp short 0213eh                          ; eb 0a
    mov BL, strict byte 026h                  ; b3 26
    jmp short 0213eh                          ; eb 06
    mov BL, strict byte 067h                  ; b3 67
    jmp short 0213eh                          ; eb 02
    mov BL, strict byte 070h                  ; b3 70
    mov al, bl                                ; 88 d8
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    xor ah, ah                                ; 30 e4
    call 0165ch                               ; e8 15 f5
    xor ah, ah                                ; 30 e4
    mov dx, ax                                ; 89 c2
    sal dx, 008h                              ; c1 e2 08
    movzx ax, bl                              ; 0f b6 c3
    call 0165ch                               ; e8 08 f5
    xor ah, ah                                ; 30 e4
    add ax, dx                                ; 01 d0
    mov word [bp-030h], ax                    ; 89 46 d0
    mov al, bl                                ; 88 d8
    add AL, strict byte 002h                  ; 04 02
    xor ah, ah                                ; 30 e4
    call 0165ch                               ; e8 f8 f4
    xor ah, ah                                ; 30 e4
    mov word [bp-032h], ax                    ; 89 46 ce
    mov al, bl                                ; 88 d8
    add AL, strict byte 007h                  ; 04 07
    xor ah, ah                                ; 30 e4
    call 0165ch                               ; e8 ea f4
    xor ah, ah                                ; 30 e4
    mov word [bp-02eh], ax                    ; 89 46 d2
    jmp short 02186h                          ; eb 0d
    mov bx, word [bp-01ch]                    ; 8b 5e e4
    mov cx, si                                ; 89 f1
    mov dx, ss                                ; 8c d2
    lea ax, [bp-032h]                         ; 8d 46 ce
    call 052a4h                               ; e8 1e 31
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 1d f7
    mov ax, word [bp-02eh]                    ; 8b 46 d2
    push ax                                   ; 50
    mov ax, word [bp-032h]                    ; 8b 46 ce
    push ax                                   ; 50
    mov ax, word [bp-030h]                    ; 8b 46 d0
    push ax                                   ; 50
    push word [bp-01ah]                       ; ff 76 e6
    push word [bp-018h]                       ; ff 76 e8
    push word [bp-016h]                       ; ff 76 ea
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc
    push ax                                   ; 50
    movzx ax, byte [bp-014h]                  ; 0f b6 46 ec
    push ax                                   ; 50
    push 000bdh                               ; 68 bd 00
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 31 f7
    add sp, strict byte 00014h                ; 83 c4 14
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    les di, [bp-024h]                         ; c4 7e dc
    add di, ax                                ; 01 c7
    mov byte [es:di+01fh], 0ffh               ; 26 c6 45 1f ff
    mov al, byte [bp-010h]                    ; 8a 46 f0
    mov byte [es:di+020h], al                 ; 26 88 45 20
    mov al, byte [bp-012h]                    ; 8a 46 ee
    mov byte [es:di+022h], al                 ; 26 88 45 22
    mov ax, word [bp-01eh]                    ; 8b 46 e2
    mov word [es:di+024h], ax                 ; 26 89 45 24
    mov ax, word [bp-018h]                    ; 8b 46 e8
    mov word [es:di+02ch], ax                 ; 26 89 45 2c
    mov ax, word [bp-016h]                    ; 8b 46 ea
    mov word [es:di+02eh], ax                 ; 26 89 45 2e
    mov ax, word [bp-01ah]                    ; 8b 46 e6
    mov word [es:di+030h], ax                 ; 26 89 45 30
    mov ax, word [bp-01ch]                    ; 8b 46 e4
    mov word [es:di+032h], ax                 ; 26 89 45 32
    mov word [es:di+034h], si                 ; 26 89 75 34
    lea di, [di+026h]                         ; 8d 7d 26
    push DS                                   ; 1e
    push SS                                   ; 16
    pop DS                                    ; 1f
    lea si, [bp-032h]                         ; 8d 76 ce
    movsw                                     ; a5
    movsw                                     ; a5
    movsw                                     ; a5
    pop DS                                    ; 1f
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    cmp AL, strict byte 002h                  ; 3c 02
    jnc short 02275h                          ; 73 60
    test al, al                               ; 84 c0
    jne short 0221eh                          ; 75 05
    mov di, strict word 0003dh                ; bf 3d 00
    jmp short 02221h                          ; eb 03
    mov di, strict word 0004dh                ; bf 4d 00
    mov dx, word [bp-020h]                    ; 8b 56 e0
    mov ax, word [bp-030h]                    ; 8b 46 d0
    mov es, dx                                ; 8e c2
    mov word [es:di], ax                      ; 26 89 05
    mov al, byte [bp-032h]                    ; 8a 46 ce
    mov byte [es:di+002h], al                 ; 26 88 45 02
    mov byte [es:di+003h], 0a0h               ; 26 c6 45 03 a0
    mov al, byte [bp-01ah]                    ; 8a 46 e6
    mov byte [es:di+004h], al                 ; 26 88 45 04
    mov ax, word [bp-016h]                    ; 8b 46 ea
    mov word [es:di+009h], ax                 ; 26 89 45 09
    mov al, byte [bp-018h]                    ; 8a 46 e8
    mov byte [es:di+00bh], al                 ; 26 88 45 0b
    mov al, byte [bp-01ah]                    ; 8a 46 e6
    mov byte [es:di+00eh], al                 ; 26 88 45 0e
    xor al, al                                ; 30 c0
    xor ah, ah                                ; 30 e4
    jmp short 0225fh                          ; eb 05
    cmp ah, 00fh                              ; 80 fc 0f
    jnc short 0226dh                          ; 73 0e
    movzx bx, ah                              ; 0f b6 dc
    mov es, dx                                ; 8e c2
    add bx, di                                ; 01 fb
    add al, byte [es:bx]                      ; 26 02 07
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4
    jmp short 0225ah                          ; eb ed
    neg al                                    ; f6 d8
    mov es, dx                                ; 8e c2
    mov byte [es:di+00fh], al                 ; 26 88 45 0f
    movzx bx, byte [bp-002h]                  ; 0f b6 5e fe
    mov es, [bp-022h]                         ; 8e 46 de
    add bx, word [bp-024h]                    ; 03 5e dc
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    mov byte [es:bx+0019fh], al               ; 26 88 87 9f 01
    inc byte [bp-002h]                        ; fe 46 fe
    cmp byte [bp-00ch], 003h                  ; 80 7e f4 03
    jne near 0232ah                           ; 0f 85 98 00
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    les bx, [bp-024h]                         ; c4 5e dc
    add bx, ax                                ; 01 c3
    mov byte [es:bx+01fh], 005h               ; 26 c6 47 1f 05
    mov byte [es:bx+022h], 000h               ; 26 c6 47 22 00
    lea dx, [bp-0025ch]                       ; 8d 96 a4 fd
    mov bx, word [bp-024h]                    ; 8b 5e dc
    mov word [es:bx+004h], dx                 ; 26 89 57 04
    mov [es:bx+006h], ss                      ; 26 8c 57 06
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    mov byte [es:bx+008h], al                 ; 26 88 47 08
    mov cx, strict word 00001h                ; b9 01 00
    mov bx, 000a1h                            ; bb a1 00
    mov ax, word [bp-024h]                    ; 8b 46 dc
    mov dx, es                                ; 8c c2
    call 01cc0h                               ; e8 f4 f9
    test ax, ax                               ; 85 c0
    je short 022dbh                           ; 74 0b
    push 000e4h                               ; 68 e4 00
    push strict byte 00007h                   ; 6a 07
    call 018e9h                               ; e8 11 f6
    add sp, strict byte 00004h                ; 83 c4 04
    mov cl, byte [bp-0025bh]                  ; 8a 8e a5 fd
    and cl, 01fh                              ; 80 e1 1f
    test byte [bp-0025ch], 080h               ; f6 86 a4 fd 80
    db  00fh, 095h, 0c0h
    ; setne al                                  ; 0f 95 c0
    xor ah, ah                                ; 30 e4
    mov dx, ax                                ; 89 c2
    cmp byte [bp-001fch], 000h                ; 80 be 04 fe 00
    db  00fh, 095h, 0c0h
    ; setne al                                  ; 0f 95 c0
    xor ah, ah                                ; 30 e4
    mov bx, ax                                ; 89 c3
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    les si, [bp-024h]                         ; c4 76 dc
    add si, ax                                ; 01 c6
    mov byte [es:si+01fh], cl                 ; 26 88 4c 1f
    mov byte [es:si+020h], dl                 ; 26 88 54 20
    mov byte [es:si+022h], bl                 ; 26 88 5c 22
    mov word [es:si+024h], 00800h             ; 26 c7 44 24 00 08
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa
    add bx, word [bp-024h]                    ; 03 5e dc
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    mov byte [es:bx+001b0h], al               ; 26 88 87 b0 01
    inc byte [bp-006h]                        ; fe 46 fa
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    cmp AL, strict byte 003h                  ; 3c 03
    je short 0235dh                           ; 74 2c
    cmp AL, strict byte 002h                  ; 3c 02
    jne near 023c0h                           ; 0f 85 89 00
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6
    imul bx, bx, strict byte 00018h           ; 6b db 18
    mov es, [bp-022h]                         ; 8e 46 de
    add bx, word [bp-024h]                    ; 03 5e dc
    mov ax, word [es:bx+032h]                 ; 26 8b 47 32
    mov word [bp-02ch], ax                    ; 89 46 d4
    mov ax, word [es:bx+034h]                 ; 26 8b 47 34
    mov word [bp-02ah], ax                    ; 89 46 d6
    mov cx, strict word 0000bh                ; b9 0b 00
    shr word [bp-02ah], 1                     ; d1 6e d6
    rcr word [bp-02ch], 1                     ; d1 5e d4
    loop 02355h                               ; e2 f8
    movzx dx, byte [bp-001bbh]                ; 0f b6 96 45 fe
    sal dx, 008h                              ; c1 e2 08
    movzx ax, byte [bp-001bch]                ; 0f b6 86 44 fe
    or dx, ax                                 ; 09 c2
    mov byte [bp-00eh], 00fh                  ; c6 46 f2 0f
    jmp short 0237bh                          ; eb 09
    dec byte [bp-00eh]                        ; fe 4e f2
    cmp byte [bp-00eh], 000h                  ; 80 7e f2 00
    jbe short 02388h                          ; 76 0d
    movzx cx, byte [bp-00eh]                  ; 0f b6 4e f2
    mov ax, strict word 00001h                ; b8 01 00
    sal ax, CL                                ; d3 e0
    test dx, ax                               ; 85 c2
    je short 02372h                           ; 74 ea
    xor di, di                                ; 31 ff
    jmp short 02391h                          ; eb 05
    cmp di, strict byte 00014h                ; 83 ff 14
    jnl short 023a6h                          ; 7d 15
    mov si, di                                ; 89 fe
    add si, di                                ; 01 fe
    mov al, byte [bp+si-00225h]               ; 8a 82 db fd
    mov byte [bp+si-05ch], al                 ; 88 42 a4
    mov al, byte [bp+si-00226h]               ; 8a 82 da fd
    mov byte [bp+si-05bh], al                 ; 88 42 a5
    inc di                                    ; 47
    jmp short 0238ch                          ; eb e6
    mov byte [bp-034h], 000h                  ; c6 46 cc 00
    mov di, strict word 00027h                ; bf 27 00
    jmp short 023b4h                          ; eb 05
    dec di                                    ; 4f
    test di, di                               ; 85 ff
    jle short 023c0h                          ; 7e 0c
    cmp byte [bp+di-05ch], 020h               ; 80 7b a4 20
    jne short 023c0h                          ; 75 06
    mov byte [bp+di-05ch], 000h               ; c6 43 a4 00
    jmp short 023afh                          ; eb ef
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    cmp AL, strict byte 003h                  ; 3c 03
    je short 02423h                           ; 74 5c
    cmp AL, strict byte 002h                  ; 3c 02
    je short 023d4h                           ; 74 09
    cmp AL, strict byte 001h                  ; 3c 01
    je near 0248ah                            ; 0f 84 b9 00
    jmp near 024a9h                           ; e9 d5 00
    cmp byte [bp-004h], 000h                  ; 80 7e fc 00
    je short 023dfh                           ; 74 05
    mov ax, 0010fh                            ; b8 0f 01
    jmp short 023e2h                          ; eb 03
    mov ax, 00116h                            ; b8 16 01
    push ax                                   ; 50
    movzx ax, byte [bp-014h]                  ; 0f b6 46 ec
    push ax                                   ; 50
    push 0011dh                               ; 68 1d 01
    push strict byte 00002h                   ; 6a 02
    call 018e9h                               ; e8 f9 f4
    add sp, strict byte 00008h                ; 83 c4 08
    xor di, di                                ; 31 ff
    movzx ax, byte [bp+di-05ch]               ; 0f b6 43 a4
    inc di                                    ; 47
    test ax, ax                               ; 85 c0
    je short 0240ch                           ; 74 0e
    push ax                                   ; 50
    push 00128h                               ; 68 28 01
    push strict byte 00002h                   ; 6a 02
    call 018e9h                               ; e8 e2 f4
    add sp, strict byte 00006h                ; 83 c4 06
    jmp short 023f5h                          ; eb e9
    push dword [bp-02ch]                      ; 66 ff 76 d4
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2
    push ax                                   ; 50
    push 0012bh                               ; 68 2b 01
    push strict byte 00002h                   ; 6a 02
    call 018e9h                               ; e8 cc f4
    add sp, strict byte 0000ah                ; 83 c4 0a
    jmp near 024a9h                           ; e9 86 00
    cmp byte [bp-004h], 000h                  ; 80 7e fc 00
    je short 0242eh                           ; 74 05
    mov ax, 0010fh                            ; b8 0f 01
    jmp short 02431h                          ; eb 03
    mov ax, 00116h                            ; b8 16 01
    push ax                                   ; 50
    movzx ax, byte [bp-014h]                  ; 0f b6 46 ec
    push ax                                   ; 50
    push 0011dh                               ; 68 1d 01
    push strict byte 00002h                   ; 6a 02
    call 018e9h                               ; e8 aa f4
    add sp, strict byte 00008h                ; 83 c4 08
    xor di, di                                ; 31 ff
    movzx ax, byte [bp+di-05ch]               ; 0f b6 43 a4
    inc di                                    ; 47
    test ax, ax                               ; 85 c0
    je short 0245bh                           ; 74 0e
    push ax                                   ; 50
    push 00128h                               ; 68 28 01
    push strict byte 00002h                   ; 6a 02
    call 018e9h                               ; e8 93 f4
    add sp, strict byte 00006h                ; 83 c4 06
    jmp short 02444h                          ; eb e9
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    les bx, [bp-024h]                         ; c4 5e dc
    add bx, ax                                ; 01 c3
    cmp byte [es:bx+01fh], 005h               ; 26 80 7f 1f 05
    jne short 02478h                          ; 75 0a
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2
    push ax                                   ; 50
    push 0014bh                               ; 68 4b 01
    jmp short 02480h                          ; eb 08
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2
    push ax                                   ; 50
    push 00165h                               ; 68 65 01
    push strict byte 00002h                   ; 6a 02
    call 018e9h                               ; e8 64 f4
    add sp, strict byte 00006h                ; 83 c4 06
    jmp short 024a9h                          ; eb 1f
    cmp byte [bp-004h], 000h                  ; 80 7e fc 00
    je short 02495h                           ; 74 05
    mov ax, 0010fh                            ; b8 0f 01
    jmp short 02498h                          ; eb 03
    mov ax, 00116h                            ; b8 16 01
    push ax                                   ; 50
    movzx ax, byte [bp-014h]                  ; 0f b6 46 ec
    push ax                                   ; 50
    push 00177h                               ; 68 77 01
    push strict byte 00002h                   ; 6a 02
    call 018e9h                               ; e8 43 f4
    add sp, strict byte 00008h                ; 83 c4 08
    inc byte [bp-00ah]                        ; fe 46 f6
    cmp byte [bp-00ah], 008h                  ; 80 7e f6 08
    jnc short 02500h                          ; 73 4e
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6
    mov ax, bx                                ; 89 d8
    cwd                                       ; 99
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2
    sar ax, 1                                 ; d1 f8
    mov word [bp-026h], ax                    ; 89 46 da
    mov al, byte [bp-026h]                    ; 8a 46 da
    mov byte [bp-014h], al                    ; 88 46 ec
    mov ax, bx                                ; 89 d8
    cwd                                       ; 99
    mov bx, strict word 00002h                ; bb 02 00
    idiv bx                                   ; f7 fb
    mov cx, dx                                ; 89 d1
    mov byte [bp-004h], dl                    ; 88 56 fc
    movzx ax, byte [bp-026h]                  ; 0f b6 46 da
    imul ax, ax, strict byte 00006h           ; 6b c0 06
    les bx, [bp-024h]                         ; c4 5e dc
    add bx, ax                                ; 01 c3
    mov si, word [es:bx+001c2h]               ; 26 8b b7 c2 01
    mov ax, word [es:bx+001c4h]               ; 26 8b 87 c4 01
    mov word [bp-028h], ax                    ; 89 46 d8
    mov dx, ax                                ; 89 c2
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 00ah                  ; b0 0a
    out DX, AL                                ; ee
    test cl, cl                               ; 84 c9
    je near 01f6ah                            ; 0f 84 70 fa
    mov ax, 000b0h                            ; b8 b0 00
    jmp near 01f6dh                           ; e9 6d fa
    mov al, byte [bp-002h]                    ; 8a 46 fe
    les bx, [bp-024h]                         ; c4 5e dc
    mov byte [es:bx+0019eh], al               ; 26 88 87 9e 01
    mov al, byte [bp-006h]                    ; 8a 46 fa
    mov byte [es:bx+001afh], al               ; 26 88 87 af 01
    movzx bx, byte [bp-002h]                  ; 0f b6 5e fe
    mov dx, strict word 00075h                ; ba 75 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 ee f0
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
ata_cmd_data_out_:                           ; 0xf2524 LB 0x215
    push si                                   ; 56
    push di                                   ; 57
    enter 0001ah, 000h                        ; c8 1a 00 00
    mov di, ax                                ; 89 c7
    mov word [bp-006h], dx                    ; 89 56 fa
    mov word [bp-016h], bx                    ; 89 5e ea
    mov word [bp-00ah], cx                    ; 89 4e f6
    mov es, dx                                ; 8e c2
    movzx ax, byte [es:di+008h]               ; 26 0f b6 45 08
    mov dx, ax                                ; 89 c2
    shr dx, 1                                 ; d1 ea
    mov dh, al                                ; 88 c6
    and dh, 001h                              ; 80 e6 01
    mov byte [bp-002h], dh                    ; 88 76 fe
    movzx si, dl                              ; 0f b6 f2
    imul si, si, strict byte 00006h           ; 6b f6 06
    add si, di                                ; 01 fe
    mov bx, word [es:si+001c2h]               ; 26 8b 9c c2 01
    mov dx, word [es:si+001c4h]               ; 26 8b 94 c4 01
    mov word [bp-008h], dx                    ; 89 56 f8
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    mov si, di                                ; 89 fe
    add si, ax                                ; 01 c6
    mov al, byte [es:si+022h]                 ; 26 8a 44 22
    mov byte [bp-004h], al                    ; 88 46 fc
    cmp AL, strict byte 001h                  ; 3c 01
    jne short 02576h                          ; 75 07
    mov word [bp-00eh], 00080h                ; c7 46 f2 80 00
    jmp short 0257bh                          ; eb 05
    mov word [bp-00eh], 00100h                ; c7 46 f2 00 01
    lea dx, [bx+007h]                         ; 8d 57 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 080h                 ; a8 80
    je short 02594h                           ; 74 0f
    mov dx, word [bp-008h]                    ; 8b 56 f8
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00001h                ; ba 01 00
    jmp near 02733h                           ; e9 9f 01
    mov es, [bp-006h]                         ; 8e 46 fa
    mov ax, word [es:di]                      ; 26 8b 05
    mov word [bp-01ah], ax                    ; 89 46 e6
    mov ax, word [es:di+002h]                 ; 26 8b 45 02
    mov word [bp-018h], ax                    ; 89 46 e8
    mov si, word [es:di+004h]                 ; 26 8b 75 04
    mov ax, word [es:di+006h]                 ; 26 8b 45 06
    mov word [bp-014h], ax                    ; 89 46 ec
    mov ax, word [es:di+012h]                 ; 26 8b 45 12
    mov word [bp-00ch], ax                    ; 89 46 f4
    mov ax, word [es:di+00eh]                 ; 26 8b 45 0e
    mov word [bp-012h], ax                    ; 89 46 ee
    mov ax, word [es:di+010h]                 ; 26 8b 45 10
    mov word [bp-010h], ax                    ; 89 46 f0
    mov ax, word [bp-00ch]                    ; 8b 46 f4
    test ax, ax                               ; 85 c0
    jne short 02632h                          ; 75 67
    mov dx, word [bp-01ah]                    ; 8b 56 e6
    add dx, word [bp-00ah]                    ; 03 56 f6
    adc ax, word [bp-018h]                    ; 13 46 e8
    cmp ax, 01000h                            ; 3d 00 10
    jnbe short 025dbh                         ; 77 02
    jne short 02606h                          ; 75 2b
    mov ax, word [bp-018h]                    ; 8b 46 e8
    xor al, al                                ; 30 c0
    shr ax, 008h                              ; c1 e8 08
    mov word [bp-00ch], ax                    ; 89 46 f4
    mov ax, word [bp-00ah]                    ; 8b 46 f6
    xor al, al                                ; 30 c0
    shr ax, 008h                              ; c1 e8 08
    lea dx, [bx+002h]                         ; 8d 57 02
    out DX, AL                                ; ee
    lea dx, [bx+003h]                         ; 8d 57 03
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    out DX, AL                                ; ee
    lea dx, [bx+004h]                         ; 8d 57 04
    xor al, al                                ; 30 c0
    out DX, AL                                ; ee
    lea dx, [bx+005h]                         ; 8d 57 05
    out DX, AL                                ; ee
    mov byte [bp-017h], al                    ; 88 46 e9
    mov ax, word [bp-01ah]                    ; 8b 46 e6
    xor ah, ah                                ; 30 e4
    mov word [bp-00ch], ax                    ; 89 46 f4
    mov cx, strict word 00008h                ; b9 08 00
    shr word [bp-018h], 1                     ; d1 6e e8
    rcr word [bp-01ah], 1                     ; d1 5e e6
    loop 02611h                               ; e2 f8
    mov ax, word [bp-01ah]                    ; 8b 46 e6
    mov word [bp-012h], ax                    ; 89 46 ee
    mov ax, word [bp-018h]                    ; 8b 46 e8
    mov word [bp-01ah], ax                    ; 89 46 e6
    mov word [bp-018h], strict word 00000h    ; c7 46 e8 00 00
    and ax, strict word 0000fh                ; 25 0f 00
    or AL, strict byte 040h                   ; 0c 40
    mov word [bp-010h], ax                    ; 89 46 f0
    mov dx, word [bp-008h]                    ; 8b 56 f8
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 00ah                  ; b0 0a
    out DX, AL                                ; ee
    lea dx, [bx+001h]                         ; 8d 57 01
    xor al, al                                ; 30 c0
    out DX, AL                                ; ee
    lea dx, [bx+002h]                         ; 8d 57 02
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    out DX, AL                                ; ee
    lea dx, [bx+003h]                         ; 8d 57 03
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    out DX, AL                                ; ee
    mov ax, word [bp-012h]                    ; 8b 46 ee
    lea dx, [bx+004h]                         ; 8d 57 04
    out DX, AL                                ; ee
    shr ax, 008h                              ; c1 e8 08
    lea dx, [bx+005h]                         ; 8d 57 05
    out DX, AL                                ; ee
    cmp byte [bp-002h], 000h                  ; 80 7e fe 00
    je short 02668h                           ; 74 05
    mov ax, 000b0h                            ; b8 b0 00
    jmp short 0266bh                          ; eb 03
    mov ax, 000a0h                            ; b8 a0 00
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0
    or ax, dx                                 ; 09 d0
    lea dx, [bx+006h]                         ; 8d 57 06
    out DX, AL                                ; ee
    lea dx, [bx+007h]                         ; 8d 57 07
    mov al, byte [bp-016h]                    ; 8a 46 ea
    out DX, AL                                ; ee
    lea dx, [bx+007h]                         ; 8d 57 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov dl, al                                ; 88 c2
    test AL, strict byte 080h                 ; a8 80
    jne short 0267ch                          ; 75 f4
    test AL, strict byte 001h                 ; a8 01
    je short 0269bh                           ; 74 0f
    mov dx, word [bp-008h]                    ; 8b 56 f8
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00002h                ; ba 02 00
    jmp near 02733h                           ; e9 98 00
    test dl, 008h                             ; f6 c2 08
    jne short 026afh                          ; 75 0f
    mov dx, word [bp-008h]                    ; 8b 56 f8
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00003h                ; ba 03 00
    jmp near 02733h                           ; e9 84 00
    sti                                       ; fb
    cmp si, 0f800h                            ; 81 fe 00 f8
    jc short 026c3h                           ; 72 0d
    sub si, 00800h                            ; 81 ee 00 08
    mov ax, word [bp-014h]                    ; 8b 46 ec
    add ax, 00080h                            ; 05 80 00
    mov word [bp-014h], ax                    ; 89 46 ec
    cmp byte [bp-004h], 001h                  ; 80 7e fc 01
    jne short 026d7h                          ; 75 0e
    mov dx, bx                                ; 89 da
    mov cx, word [bp-00eh]                    ; 8b 4e f2
    mov es, [bp-014h]                         ; 8e 46 ec
    db  0f3h, 066h, 026h, 06fh
    ; rep es outsd                              ; f3 66 26 6f
    jmp short 026e2h                          ; eb 0b
    mov dx, bx                                ; 89 da
    mov cx, word [bp-00eh]                    ; 8b 4e f2
    mov es, [bp-014h]                         ; 8e 46 ec
    db  0f3h, 026h, 06fh
    ; rep es outsw                              ; f3 26 6f
    mov es, [bp-006h]                         ; 8e 46 fa
    inc word [es:di+014h]                     ; 26 ff 45 14
    dec word [bp-00ah]                        ; ff 4e f6
    lea dx, [bx+007h]                         ; 8d 57 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov dl, al                                ; 88 c2
    test AL, strict byte 080h                 ; a8 80
    jne short 026ech                          ; 75 f4
    cmp word [bp-00ah], strict byte 00000h    ; 83 7e f6 00
    jne short 02712h                          ; 75 14
    and AL, strict byte 0e9h                  ; 24 e9
    cmp AL, strict byte 040h                  ; 3c 40
    je short 02728h                           ; 74 24
    mov dx, word [bp-008h]                    ; 8b 56 f8
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00006h                ; ba 06 00
    jmp short 02733h                          ; eb 21
    mov al, dl                                ; 88 d0
    and AL, strict byte 0c9h                  ; 24 c9
    cmp AL, strict byte 048h                  ; 3c 48
    je short 026b0h                           ; 74 96
    mov dx, word [bp-008h]                    ; 8b 56 f8
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00007h                ; ba 07 00
    jmp short 02733h                          ; eb 0b
    mov dx, word [bp-008h]                    ; 8b 56 f8
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    xor dx, dx                                ; 31 d2
    mov ax, dx                                ; 89 d0
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
@ata_read_sectors:                           ; 0xf2739 LB 0x77
    push si                                   ; 56
    push di                                   ; 57
    enter 00002h, 000h                        ; c8 02 00 00
    mov si, word [bp+008h]                    ; 8b 76 08
    mov es, [bp+00ah]                         ; 8e 46 0a
    mov bl, byte [es:si+008h]                 ; 26 8a 5c 08
    mov cx, word [es:si+00ah]                 ; 26 8b 4c 0a
    mov dx, cx                                ; 89 ca
    sal dx, 009h                              ; c1 e2 09
    mov ax, word [es:si+012h]                 ; 26 8b 44 12
    test ax, ax                               ; 85 c0
    je short 02767h                           ; 74 0d
    movzx di, bl                              ; 0f b6 fb
    imul di, di, strict byte 00018h           ; 6b ff 18
    mov [bp-002h], es                         ; 8c 46 fe
    add di, si                                ; 01 f7
    jmp short 02793h                          ; eb 2c
    mov di, word [es:si]                      ; 26 8b 3c
    add di, cx                                ; 01 cf
    mov word [bp-002h], di                    ; 89 7e fe
    adc ax, word [es:si+002h]                 ; 26 13 44 02
    cmp ax, 01000h                            ; 3d 00 10
    jnbe short 0277ah                         ; 77 02
    jne short 02786h                          ; 75 0c
    mov bx, strict word 00024h                ; bb 24 00
    mov ax, si                                ; 89 f0
    mov dx, es                                ; 8c c2
    call 01cc0h                               ; e8 3c f5
    jmp short 027aah                          ; eb 24
    movzx ax, bl                              ; 0f b6 c3
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    mov [bp-002h], es                         ; 8c 46 fe
    mov di, si                                ; 89 f7
    add di, ax                                ; 01 c7
    mov word [es:di+024h], dx                 ; 26 89 55 24
    mov bx, 000c4h                            ; bb c4 00
    mov ax, si                                ; 89 f0
    mov dx, es                                ; 8c c2
    call 01cc0h                               ; e8 1f f5
    mov es, [bp-002h]                         ; 8e 46 fe
    mov word [es:di+024h], 00200h             ; 26 c7 45 24 00 02
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn 00004h                               ; c2 04 00
@ata_write_sectors:                          ; 0xf27b0 LB 0x3a
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    les si, [bp+006h]                         ; c4 76 06
    mov cx, word [es:si+00ah]                 ; 26 8b 4c 0a
    cmp word [es:si+012h], strict byte 00000h ; 26 83 7c 12 00
    je short 027ceh                           ; 74 0c
    mov bx, strict word 00030h                ; bb 30 00
    mov ax, si                                ; 89 f0
    mov dx, es                                ; 8c c2
    call 02524h                               ; e8 58 fd
    jmp short 027e5h                          ; eb 17
    xor ax, ax                                ; 31 c0
    mov dx, word [es:si]                      ; 26 8b 14
    add dx, cx                                ; 01 ca
    adc ax, word [es:si+002h]                 ; 26 13 44 02
    cmp ax, 01000h                            ; 3d 00 10
    jnbe short 027e0h                         ; 77 02
    jne short 027c2h                          ; 75 e2
    mov bx, strict word 00034h                ; bb 34 00
    jmp short 027c5h                          ; eb e0
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn 00004h                               ; c2 04 00
ata_cmd_packet_:                             ; 0xf27ea LB 0x2e3
    push si                                   ; 56
    push di                                   ; 57
    enter 00014h, 000h                        ; c8 14 00 00
    push ax                                   ; 50
    mov byte [bp-004h], dl                    ; 88 56 fc
    mov di, bx                                ; 89 df
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 1d ee
    mov word [bp-00eh], 00122h                ; c7 46 f2 22 01
    mov word [bp-00ah], ax                    ; 89 46 f6
    mov ax, word [bp-016h]                    ; 8b 46 ea
    shr ax, 1                                 ; d1 e8
    mov ah, byte [bp-016h]                    ; 8a 66 ea
    and ah, 001h                              ; 80 e4 01
    mov byte [bp-002h], ah                    ; 88 66 fe
    cmp byte [bp+00eh], 002h                  ; 80 7e 0e 02
    jne short 0283ah                          ; 75 1f
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 88 f0
    push 00191h                               ; 68 91 01
    push 001a0h                               ; 68 a0 01
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 b8 f0
    add sp, strict byte 00006h                ; 83 c4 06
    mov dx, strict word 00001h                ; ba 01 00
    jmp near 02ac5h                           ; e9 8b 02
    test byte [bp+008h], 001h                 ; f6 46 08 01
    jne short 02834h                          ; 75 f4
    xor ah, ah                                ; 30 e4
    imul ax, ax, strict byte 00006h           ; 6b c0 06
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov si, word [bp-00eh]                    ; 8b 76 f2
    add si, ax                                ; 01 c6
    mov bx, word [es:si+001c2h]               ; 26 8b 9c c2 01
    mov ax, word [es:si+001c4h]               ; 26 8b 84 c4 01
    mov word [bp-00ch], ax                    ; 89 46 f4
    imul si, word [bp-016h], strict byte 00018h ; 6b 76 ea 18
    add si, word [bp-00eh]                    ; 03 76 f2
    mov al, byte [es:si+022h]                 ; 26 8a 44 22
    mov byte [bp-006h], al                    ; 88 46 fa
    xor ax, ax                                ; 31 c0
    mov word [bp-014h], ax                    ; 89 46 ec
    mov word [bp-012h], ax                    ; 89 46 ee
    mov al, byte [bp-004h]                    ; 8a 46 fc
    cmp AL, strict byte 00ch                  ; 3c 0c
    jnc short 0287dh                          ; 73 06
    mov byte [bp-004h], 00ch                  ; c6 46 fc 0c
    jmp short 02883h                          ; eb 06
    jbe short 02883h                          ; 76 04
    mov byte [bp-004h], 010h                  ; c6 46 fc 10
    shr byte [bp-004h], 1                     ; d0 6e fc
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov si, word [bp-00eh]                    ; 8b 76 f2
    db  066h, 026h, 0c7h, 044h, 014h, 000h, 000h, 000h, 000h
    ; mov dword [es:si+014h], strict dword 000000000h ; 66 26 c7 44 14 00 00 00 00
    mov word [es:si+018h], strict word 00000h ; 26 c7 44 18 00 00
    lea dx, [bx+007h]                         ; 8d 57 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 080h                 ; a8 80
    je short 028abh                           ; 74 06
    mov dx, strict word 00002h                ; ba 02 00
    jmp near 02ac5h                           ; e9 1a 02
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 00ah                  ; b0 0a
    out DX, AL                                ; ee
    lea dx, [bx+004h]                         ; 8d 57 04
    mov AL, strict byte 0f0h                  ; b0 f0
    out DX, AL                                ; ee
    lea dx, [bx+005h]                         ; 8d 57 05
    mov AL, strict byte 0ffh                  ; b0 ff
    out DX, AL                                ; ee
    cmp byte [bp-002h], 000h                  ; 80 7e fe 00
    je short 028cbh                           ; 74 05
    mov ax, 000b0h                            ; b8 b0 00
    jmp short 028ceh                          ; eb 03
    mov ax, 000a0h                            ; b8 a0 00
    lea dx, [bx+006h]                         ; 8d 57 06
    out DX, AL                                ; ee
    lea dx, [bx+007h]                         ; 8d 57 07
    mov AL, strict byte 0a0h                  ; b0 a0
    out DX, AL                                ; ee
    lea dx, [bx+007h]                         ; 8d 57 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov dl, al                                ; 88 c2
    test AL, strict byte 080h                 ; a8 80
    jne short 028d8h                          ; 75 f4
    test AL, strict byte 001h                 ; a8 01
    je short 028f7h                           ; 74 0f
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00003h                ; ba 03 00
    jmp near 02ac5h                           ; e9 ce 01
    test dl, 008h                             ; f6 c2 08
    jne short 0290bh                          ; 75 0f
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00004h                ; ba 04 00
    jmp near 02ac5h                           ; e9 ba 01
    sti                                       ; fb
    mov ax, di                                ; 89 f8
    shr ax, 004h                              ; c1 e8 04
    add ax, cx                                ; 01 c8
    mov si, di                                ; 89 fe
    and si, strict byte 0000fh                ; 83 e6 0f
    movzx cx, byte [bp-004h]                  ; 0f b6 4e fc
    mov dx, bx                                ; 89 da
    mov es, ax                                ; 8e c0
    db  0f3h, 026h, 06fh
    ; rep es outsw                              ; f3 26 6f
    cmp byte [bp+00eh], 000h                  ; 80 7e 0e 00
    jne short 02934h                          ; 75 0b
    lea dx, [bx+007h]                         ; 8d 57 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov dl, al                                ; 88 c2
    jmp near 02aa6h                           ; e9 72 01
    lea dx, [bx+007h]                         ; 8d 57 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov dl, al                                ; 88 c2
    test AL, strict byte 080h                 ; a8 80
    jne short 02934h                          ; 75 f4
    test AL, strict byte 088h                 ; a8 88
    je near 02aa6h                            ; 0f 84 60 01
    test AL, strict byte 001h                 ; a8 01
    je short 02955h                           ; 74 0b
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    jmp short 028f1h                          ; eb 9c
    mov al, dl                                ; 88 d0
    and AL, strict byte 0c9h                  ; 24 c9
    cmp AL, strict byte 048h                  ; 3c 48
    je short 02968h                           ; 74 0b
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    jmp short 02905h                          ; eb 9d
    mov ax, word [bp+010h]                    ; 8b 46 10
    shr ax, 004h                              ; c1 e8 04
    mov dx, word [bp+012h]                    ; 8b 56 12
    add dx, ax                                ; 01 c2
    mov ax, word [bp+010h]                    ; 8b 46 10
    and ax, strict word 0000fh                ; 25 0f 00
    mov word [bp+010h], ax                    ; 89 46 10
    mov word [bp+012h], dx                    ; 89 56 12
    lea dx, [bx+005h]                         ; 8d 57 05
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov cx, ax                                ; 89 c1
    sal cx, 008h                              ; c1 e1 08
    lea dx, [bx+004h]                         ; 8d 57 04
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    add cx, ax                                ; 01 c1
    mov word [bp-010h], cx                    ; 89 4e f0
    mov ax, word [bp+008h]                    ; 8b 46 08
    cmp ax, cx                                ; 39 c8
    jbe short 029a8h                          ; 76 0c
    mov ax, cx                                ; 89 c8
    sub word [bp+008h], cx                    ; 29 4e 08
    xor ax, cx                                ; 31 c8
    mov word [bp-010h], ax                    ; 89 46 f0
    jmp short 029b2h                          ; eb 0a
    mov cx, ax                                ; 89 c1
    mov word [bp+008h], strict word 00000h    ; c7 46 08 00 00
    sub word [bp-010h], ax                    ; 29 46 f0
    xor ax, ax                                ; 31 c0
    cmp word [bp+00ch], strict byte 00000h    ; 83 7e 0c 00
    jne short 029dbh                          ; 75 21
    mov dx, word [bp-010h]                    ; 8b 56 f0
    cmp dx, word [bp+00ah]                    ; 3b 56 0a
    jbe short 029dbh                          ; 76 19
    mov ax, word [bp-010h]                    ; 8b 46 f0
    sub ax, word [bp+00ah]                    ; 2b 46 0a
    mov word [bp-008h], ax                    ; 89 46 f8
    mov ax, word [bp+00ah]                    ; 8b 46 0a
    mov word [bp-010h], ax                    ; 89 46 f0
    xor ax, ax                                ; 31 c0
    mov word [bp+00ah], ax                    ; 89 46 0a
    mov word [bp+00ch], ax                    ; 89 46 0c
    jmp short 029e7h                          ; eb 0c
    mov word [bp-008h], ax                    ; 89 46 f8
    mov dx, word [bp-010h]                    ; 8b 56 f0
    sub word [bp+00ah], dx                    ; 29 56 0a
    sbb word [bp+00ch], ax                    ; 19 46 0c
    mov si, word [bp-010h]                    ; 8b 76 f0
    mov al, byte [bp-006h]                    ; 8a 46 fa
    test cl, 003h                             ; f6 c1 03
    je short 029f4h                           ; 74 02
    xor al, al                                ; 30 c0
    test byte [bp-010h], 003h                 ; f6 46 f0 03
    je short 029fch                           ; 74 02
    xor al, al                                ; 30 c0
    test byte [bp-008h], 003h                 ; f6 46 f8 03
    je short 02a04h                           ; 74 02
    xor al, al                                ; 30 c0
    test byte [bp-010h], 001h                 ; f6 46 f0 01
    je short 02a1ch                           ; 74 12
    inc word [bp-010h]                        ; ff 46 f0
    cmp word [bp-008h], strict byte 00000h    ; 83 7e f8 00
    jbe short 02a1ch                          ; 76 09
    test byte [bp-008h], 001h                 ; f6 46 f8 01
    je short 02a1ch                           ; 74 03
    dec word [bp-008h]                        ; ff 4e f8
    cmp AL, strict byte 001h                  ; 3c 01
    jne short 02a2dh                          ; 75 0d
    shr word [bp-010h], 002h                  ; c1 6e f0 02
    shr cx, 002h                              ; c1 e9 02
    shr word [bp-008h], 002h                  ; c1 6e f8 02
    jmp short 02a35h                          ; eb 08
    shr word [bp-010h], 1                     ; d1 6e f0
    shr cx, 1                                 ; d1 e9
    shr word [bp-008h], 1                     ; d1 6e f8
    cmp AL, strict byte 001h                  ; 3c 01
    jne short 02a65h                          ; 75 2c
    test cx, cx                               ; 85 c9
    je short 02a47h                           ; 74 0a
    mov dx, bx                                ; 89 da
    push eax                                  ; 66 50
    in eax, DX                                ; 66 ed
    loop 02a41h                               ; e2 fc
    pop eax                                   ; 66 58
    mov dx, bx                                ; 89 da
    mov cx, word [bp-010h]                    ; 8b 4e f0
    les di, [bp+010h]                         ; c4 7e 10
    db  0f3h, 066h, 06dh
    ; rep insd                                  ; f3 66 6d
    mov ax, word [bp-008h]                    ; 8b 46 f8
    test ax, ax                               ; 85 c0
    je short 02a84h                           ; 74 2b
    mov cx, ax                                ; 89 c1
    push eax                                  ; 66 50
    in eax, DX                                ; 66 ed
    loop 02a5dh                               ; e2 fc
    pop eax                                   ; 66 58
    jmp short 02a84h                          ; eb 1f
    test cx, cx                               ; 85 c9
    je short 02a6eh                           ; 74 05
    mov dx, bx                                ; 89 da
    in ax, DX                                 ; ed
    loop 02a6bh                               ; e2 fd
    mov dx, bx                                ; 89 da
    mov cx, word [bp-010h]                    ; 8b 4e f0
    les di, [bp+010h]                         ; c4 7e 10
    rep insw                                  ; f3 6d
    mov ax, word [bp-008h]                    ; 8b 46 f8
    test ax, ax                               ; 85 c0
    je short 02a84h                           ; 74 05
    mov cx, ax                                ; 89 c1
    in ax, DX                                 ; ed
    loop 02a81h                               ; e2 fd
    add word [bp+010h], si                    ; 01 76 10
    xor ax, ax                                ; 31 c0
    add word [bp-014h], si                    ; 01 76 ec
    adc word [bp-012h], ax                    ; 11 46 ee
    mov ax, word [bp-014h]                    ; 8b 46 ec
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov si, word [bp-00eh]                    ; 8b 76 f2
    mov word [es:si+016h], ax                 ; 26 89 44 16
    mov ax, word [bp-012h]                    ; 8b 46 ee
    mov word [es:si+018h], ax                 ; 26 89 44 18
    jmp near 02934h                           ; e9 8e fe
    mov al, dl                                ; 88 d0
    and AL, strict byte 0e9h                  ; 24 e9
    cmp AL, strict byte 040h                  ; 3c 40
    je short 02abah                           ; 74 0c
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    jmp near 02905h                           ; e9 4b fe
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    xor dx, dx                                ; 31 d2
    mov ax, dx                                ; 89 d0
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn 0000ch                               ; c2 0c 00
ata_soft_reset_:                             ; 0xf2acd LB 0x7d
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    enter 00002h, 000h                        ; c8 02 00 00
    mov bx, ax                                ; 89 c3
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 3d eb
    mov dx, bx                                ; 89 da
    shr dx, 1                                 ; d1 ea
    and bl, 001h                              ; 80 e3 01
    mov byte [bp-002h], bl                    ; 88 5e fe
    xor dh, dh                                ; 30 f6
    imul bx, dx, strict byte 00006h           ; 6b da 06
    mov es, ax                                ; 8e c0
    add bx, 00122h                            ; 81 c3 22 01
    mov cx, word [es:bx+001c2h]               ; 26 8b 8f c2 01
    mov bx, word [es:bx+001c4h]               ; 26 8b 9f c4 01
    lea dx, [bx+006h]                         ; 8d 57 06
    mov AL, strict byte 00ah                  ; b0 0a
    out DX, AL                                ; ee
    cmp byte [bp-002h], 000h                  ; 80 7e fe 00
    je short 02b0fh                           ; 74 05
    mov ax, 000b0h                            ; b8 b0 00
    jmp short 02b12h                          ; eb 03
    mov ax, 000a0h                            ; b8 a0 00
    mov dx, cx                                ; 89 ca
    add dx, strict byte 00006h                ; 83 c2 06
    out DX, AL                                ; ee
    mov dx, cx                                ; 89 ca
    add dx, strict byte 00007h                ; 83 c2 07
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, cx                                ; 89 ca
    add dx, strict byte 00007h                ; 83 c2 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 080h                 ; a8 80
    jne short 02b20h                          ; 75 f4
    and AL, strict byte 0e9h                  ; 24 e9
    cmp AL, strict byte 040h                  ; 3c 40
    je short 02b3dh                           ; 74 0b
    lea dx, [bx+006h]                         ; 8d 57 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov ax, strict word 00001h                ; b8 01 00
    jmp short 02b45h                          ; eb 08
    lea dx, [bx+006h]                         ; 8d 57 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    xor ax, ax                                ; 31 c0
    leave                                     ; c9
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
set_diskette_ret_status_:                    ; 0xf2b4a LB 0x15
    push bx                                   ; 53
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 00041h                ; ba 41 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 b3 ea
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    retn                                      ; c3
set_diskette_current_cyl_:                   ; 0xf2b5f LB 0x2a
    push bx                                   ; 53
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bl, al                                ; 88 c3
    cmp AL, strict byte 001h                  ; 3c 01
    jbe short 02b74h                          ; 76 0b
    push 001c0h                               ; 68 c0 01
    push strict byte 00007h                   ; 6a 07
    call 018e9h                               ; e8 78 ed
    add sp, strict byte 00004h                ; 83 c4 04
    movzx ax, dl                              ; 0f b6 c2
    movzx dx, bl                              ; 0f b6 d3
    add dx, 00094h                            ; 81 c2 94 00
    mov bx, ax                                ; 89 c3
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 88 ea
    pop bp                                    ; 5d
    pop bx                                    ; 5b
    retn                                      ; c3
floppy_reset_controller_:                    ; 0xf2b89 LB 0x28
    push bx                                   ; 53
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, 003f2h                            ; ba f2 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov bx, ax                                ; 89 c3
    movzx ax, bl                              ; 0f b6 c3
    and AL, strict byte 0fbh                  ; 24 fb
    out DX, AL                                ; ee
    mov al, bl                                ; 88 d8
    or AL, strict byte 004h                   ; 0c 04
    out DX, AL                                ; ee
    mov dx, 003f4h                            ; ba f4 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and AL, strict byte 0c0h                  ; 24 c0
    cmp AL, strict byte 080h                  ; 3c 80
    jne short 02ba1h                          ; 75 f4
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    retn                                      ; c3
floppy_prepare_controller_:                  ; 0xf2bb1 LB 0x8c
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    enter 00002h, 000h                        ; c8 02 00 00
    mov cx, ax                                ; 89 c1
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 3d ea
    and AL, strict byte 07fh                  ; 24 7f
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 3d ea
    mov dx, 003f2h                            ; ba f2 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and AL, strict byte 004h                  ; 24 04
    mov byte [bp-002h], al                    ; 88 46 fe
    test cx, cx                               ; 85 c9
    je short 02be4h                           ; 74 04
    mov AL, strict byte 020h                  ; b0 20
    jmp short 02be6h                          ; eb 02
    mov AL, strict byte 010h                  ; b0 10
    or AL, strict byte 00ch                   ; 0c 0c
    or al, cl                                 ; 08 c8
    mov dx, 003f2h                            ; ba f2 03
    out DX, AL                                ; ee
    mov bx, strict word 00025h                ; bb 25 00
    mov dx, strict word 00040h                ; ba 40 00
    mov ax, dx                                ; 89 d0
    call 0160eh                               ; e8 15 ea
    mov dx, 0008bh                            ; ba 8b 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 fe e9
    shr al, 006h                              ; c0 e8 06
    mov dx, 003f7h                            ; ba f7 03
    out DX, AL                                ; ee
    mov dx, 003f4h                            ; ba f4 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and AL, strict byte 0c0h                  ; 24 c0
    cmp AL, strict byte 080h                  ; 3c 80
    jne short 02c09h                          ; 75 f4
    cmp byte [bp-002h], 000h                  ; 80 7e fe 00
    jne short 02c38h                          ; 75 1d
    sti                                       ; fb
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 db e9
    test AL, strict byte 080h                 ; a8 80
    je short 02c1ch                           ; 74 f3
    and AL, strict byte 07fh                  ; 24 7f
    cli                                       ; fa
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 d6 e9
    leave                                     ; c9
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
floppy_media_known_:                         ; 0xf2c3d LB 0x40
    push bx                                   ; 53
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, ax                                ; 89 c3
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 b3 e9
    mov ah, al                                ; 88 c4
    test bx, bx                               ; 85 db
    je short 02c55h                           ; 74 02
    shr al, 1                                 ; d0 e8
    and AL, strict byte 001h                  ; 24 01
    jne short 02c5dh                          ; 75 04
    xor ah, ah                                ; 30 e4
    jmp short 02c79h                          ; eb 1c
    mov dx, 00090h                            ; ba 90 00
    test bx, bx                               ; 85 db
    je short 02c67h                           ; 74 03
    mov dx, 00091h                            ; ba 91 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 93 e9
    xor ah, ah                                ; 30 e4
    sar ax, 004h                              ; c1 f8 04
    and AL, strict byte 001h                  ; 24 01
    je short 02c59h                           ; 74 e3
    mov ax, strict word 00001h                ; b8 01 00
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    retn                                      ; c3
floppy_read_id_:                             ; 0xf2c7d LB 0x49
    push bx                                   ; 53
    push dx                                   ; 52
    push si                                   ; 56
    enter 00008h, 000h                        ; c8 08 00 00
    mov bx, ax                                ; 89 c3
    call 02bb1h                               ; e8 28 ff
    mov AL, strict byte 04ah                  ; b0 4a
    mov dx, 003f5h                            ; ba f5 03
    out DX, AL                                ; ee
    mov al, bl                                ; 88 d8
    out DX, AL                                ; ee
    sti                                       ; fb
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 64 e9
    and AL, strict byte 080h                  ; 24 80
    test al, al                               ; 84 c0
    je short 02c93h                           ; 74 f1
    cli                                       ; fa
    xor si, si                                ; 31 f6
    jmp short 02cach                          ; eb 05
    cmp si, strict byte 00007h                ; 83 fe 07
    jnl short 02cb8h                          ; 7d 0c
    mov dx, 003f5h                            ; ba f5 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov byte [bp+si-008h], al                 ; 88 42 f8
    inc si                                    ; 46
    jmp short 02ca7h                          ; eb ef
    test byte [bp-008h], 0c0h                 ; f6 46 f8 c0
    db  00fh, 094h, 0c0h
    ; sete al                                   ; 0f 94 c0
    xor ah, ah                                ; 30 e4
    leave                                     ; c9
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    retn                                      ; c3
floppy_drive_recal_:                         ; 0xf2cc6 LB 0x5e
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, ax                                ; 89 c3
    call 02bb1h                               ; e8 e0 fe
    mov AL, strict byte 007h                  ; b0 07
    mov dx, 003f5h                            ; ba f5 03
    out DX, AL                                ; ee
    mov al, bl                                ; 88 d8
    out DX, AL                                ; ee
    sti                                       ; fb
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 1c e9
    and AL, strict byte 080h                  ; 24 80
    test al, al                               ; 84 c0
    je short 02cdbh                           ; 74 f1
    cli                                       ; fa
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 0c e9
    and AL, strict byte 07fh                  ; 24 7f
    test bx, bx                               ; 85 db
    je short 02d01h                           ; 74 07
    or AL, strict byte 002h                   ; 0c 02
    mov cx, 00095h                            ; b9 95 00
    jmp short 02d06h                          ; eb 05
    or AL, strict byte 001h                   ; 0c 01
    mov cx, 00094h                            ; b9 94 00
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 fc e8
    xor bx, bx                                ; 31 db
    mov dx, cx                                ; 89 ca
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 f2 e8
    mov ax, strict word 00001h                ; b8 01 00
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
floppy_media_sense_:                         ; 0xf2d24 LB 0xfa
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov di, ax                                ; 89 c7
    call 02cc6h                               ; e8 95 ff
    test ax, ax                               ; 85 c0
    jne short 02d3ah                          ; 75 05
    xor cx, cx                                ; 31 c9
    jmp near 02e15h                           ; e9 db 00
    mov ax, strict word 00010h                ; b8 10 00
    call 0165ch                               ; e8 1c e9
    test di, di                               ; 85 ff
    jne short 02d4bh                          ; 75 07
    mov cl, al                                ; 88 c1
    shr cl, 004h                              ; c0 e9 04
    jmp short 02d50h                          ; eb 05
    mov cl, al                                ; 88 c1
    and cl, 00fh                              ; 80 e1 0f
    cmp cl, 001h                              ; 80 f9 01
    jne short 02d5eh                          ; 75 09
    xor cl, cl                                ; 30 c9
    mov CH, strict byte 015h                  ; b5 15
    mov si, strict word 00001h                ; be 01 00
    jmp short 02da9h                          ; eb 4b
    cmp cl, 002h                              ; 80 f9 02
    jne short 02d69h                          ; 75 06
    xor cl, cl                                ; 30 c9
    mov CH, strict byte 035h                  ; b5 35
    jmp short 02d59h                          ; eb f0
    cmp cl, 003h                              ; 80 f9 03
    jne short 02d74h                          ; 75 06
    xor cl, cl                                ; 30 c9
    mov CH, strict byte 017h                  ; b5 17
    jmp short 02d59h                          ; eb e5
    cmp cl, 004h                              ; 80 f9 04
    jne short 02d7fh                          ; 75 06
    xor cl, cl                                ; 30 c9
    mov CH, strict byte 017h                  ; b5 17
    jmp short 02d59h                          ; eb da
    cmp cl, 005h                              ; 80 f9 05
    jne short 02d8ah                          ; 75 06
    mov CL, strict byte 0cch                  ; b1 cc
    mov CH, strict byte 0d7h                  ; b5 d7
    jmp short 02d59h                          ; eb cf
    cmp cl, 006h                              ; 80 f9 06
    jne short 02d95h                          ; 75 06
    xor cl, cl                                ; 30 c9
    mov CH, strict byte 027h                  ; b5 27
    jmp short 02d59h                          ; eb c4
    cmp cl, 007h                              ; 80 f9 07
    jne short 02d9ch                          ; 75 02
    jmp short 02d8fh                          ; eb f3
    cmp cl, 008h                              ; 80 f9 08
    jne short 02da3h                          ; 75 02
    jmp short 02d8fh                          ; eb ec
    xor cl, cl                                ; 30 c9
    xor ch, ch                                ; 30 ed
    xor si, si                                ; 31 f6
    movzx bx, cl                              ; 0f b6 d9
    mov dx, 0008bh                            ; ba 8b 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 59 e8
    mov ax, di                                ; 89 f8
    call 02c7dh                               ; e8 c3 fe
    test ax, ax                               ; 85 c0
    jne short 02df0h                          ; 75 32
    mov al, cl                                ; 88 c8
    and AL, strict byte 0c0h                  ; 24 c0
    cmp AL, strict byte 080h                  ; 3c 80
    je short 02df0h                           ; 74 2a
    mov al, cl                                ; 88 c8
    and AL, strict byte 0c0h                  ; 24 c0
    cmp AL, strict byte 0c0h                  ; 3c c0
    je short 02dddh                           ; 74 0f
    mov ah, cl                                ; 88 cc
    and ah, 03fh                              ; 80 e4 3f
    cmp AL, strict byte 040h                  ; 3c 40
    je short 02de9h                           ; 74 12
    test al, al                               ; 84 c0
    je short 02de2h                           ; 74 07
    jmp short 02da9h                          ; eb cc
    and cl, 03fh                              ; 80 e1 3f
    jmp short 02da9h                          ; eb c7
    mov cl, ah                                ; 88 e1
    or cl, 040h                               ; 80 c9 40
    jmp short 02da9h                          ; eb c0
    mov cl, ah                                ; 88 e1
    or cl, 080h                               ; 80 c9 80
    jmp short 02da9h                          ; eb b9
    test di, di                               ; 85 ff
    db  00fh, 095h, 0c0h
    ; setne al                                  ; 0f 95 c0
    movzx di, al                              ; 0f b6 f8
    add di, 00090h                            ; 81 c7 90 00
    movzx bx, cl                              ; 0f b6 d9
    mov dx, 0008bh                            ; ba 8b 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 06 e8
    movzx bx, ch                              ; 0f b6 dd
    mov dx, di                                ; 89 fa
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 fb e7
    mov cx, si                                ; 89 f1
    mov ax, cx                                ; 89 c8
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
floppy_drive_exists_:                        ; 0xf2e1e LB 0x33
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, ax                                ; 89 c2
    mov ax, strict word 00010h                ; b8 10 00
    call 0165ch                               ; e8 32 e8
    test dx, dx                               ; 85 d2
    jne short 02e33h                          ; 75 05
    shr al, 004h                              ; c0 e8 04
    jmp short 02e35h                          ; eb 02
    and AL, strict byte 00fh                  ; 24 0f
    test al, al                               ; 84 c0
    db  00fh, 095h, 0c0h
    ; setne al                                  ; 0f 95 c0
    xor ah, ah                                ; 30 e4
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    retn                                      ; c3
    sti                                       ; fb
    xor AL, strict byte 006h                  ; 34 06
    xor ax, 03510h                            ; 35 10 35
    pop SS                                    ; 17
    xor ax, 0351eh                            ; 35 1e 35
    and ax, 02c35h                            ; 25 35 2c
    xor ax, 03536h                            ; 35 36 35
    db  03dh
    db  035h
_int13_diskette_function:                    ; 0xf2e51 LB 0x78f
    push si                                   ; 56
    push di                                   ; 57
    enter 00012h, 000h                        ; c8 12 00 00
    mov bx, word [bp+01ah]                    ; 8b 5e 1a
    shr bx, 008h                              ; c1 eb 08
    mov ch, bl                                ; 88 dd
    mov si, word [bp+01ah]                    ; 8b 76 1a
    and si, 000ffh                            ; 81 e6 ff 00
    mov ah, byte [bp+012h]                    ; 8a 66 12
    cmp bl, 008h                              ; 80 fb 08
    jc short 02ea8h                           ; 72 3a
    mov dx, word [bp+020h]                    ; 8b 56 20
    or dl, 001h                               ; 80 ca 01
    cmp bl, 008h                              ; 80 fb 08
    jbe near 0347ch                           ; 0f 86 01 06
    cmp bl, 016h                              ; 80 fb 16
    jc short 02e9eh                           ; 72 1e
    or si, 00100h                             ; 81 ce 00 01
    mov cx, si                                ; 89 f1
    cmp bl, 016h                              ; 80 fb 16
    jbe near 0359bh                           ; 0f 86 0e 07
    cmp bl, 018h                              ; 80 fb 18
    je near 035a0h                            ; 0f 84 0c 07
    cmp bl, 017h                              ; 80 fb 17
    je near 035a0h                            ; 0f 84 05 07
    jmp near 035bdh                           ; e9 1f 07
    cmp bl, 015h                              ; 80 fb 15
    je near 0355fh                            ; 0f 84 ba 06
    jmp near 035bdh                           ; e9 15 07
    cmp bl, 001h                              ; 80 fb 01
    jc short 02ec2h                           ; 72 15
    jbe near 02f38h                           ; 0f 86 87 00
    cmp bl, 005h                              ; 80 fb 05
    je near 032d5h                            ; 0f 84 1d 04
    cmp bl, 004h                              ; 80 fb 04
    jbe near 02f56h                           ; 0f 86 97 00
    jmp near 035bdh                           ; e9 fb 06
    test bl, bl                               ; 84 db
    jne near 035bdh                           ; 0f 85 f5 06
    mov al, byte [bp+012h]                    ; 8a 46 12
    mov byte [bp-00ah], al                    ; 88 46 f6
    cmp AL, strict byte 001h                  ; 3c 01
    jbe short 02ee6h                          ; 76 14
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    xor ah, ah                                ; 30 e4
    or ah, 001h                               ; 80 cc 01
    mov word [bp+01ah], ax                    ; 89 46 1a
    mov ax, strict word 00001h                ; b8 01 00
    call 02b4ah                               ; e8 67 fc
    jmp near 032b1h                           ; e9 cb 03
    mov ax, strict word 00010h                ; b8 10 00
    call 0165ch                               ; e8 70 e7
    cmp byte [bp-00ah], 000h                  ; 80 7e f6 00
    jne short 02ef9h                          ; 75 07
    mov bl, al                                ; 88 c3
    shr bl, 004h                              ; c0 eb 04
    jmp short 02efeh                          ; eb 05
    mov bl, al                                ; 88 c3
    and bl, 00fh                              ; 80 e3 0f
    test bl, bl                               ; 84 db
    jne short 02f12h                          ; 75 10
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    xor ah, ah                                ; 30 e4
    or ah, 080h                               ; 80 cc 80
    mov word [bp+01ah], ax                    ; 89 46 1a
    mov ax, 00080h                            ; b8 80 00
    jmp short 02ee0h                          ; eb ce
    xor bx, bx                                ; 31 db
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 f1 e6
    xor al, al                                ; 30 c0
    mov byte [bp+01bh], al                    ; 88 46 1b
    xor ah, ah                                ; 30 e4
    call 02b4ah                               ; e8 23 fc
    and byte [bp+020h], 0feh                  ; 80 66 20 fe
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6
    xor dx, dx                                ; 31 d2
    call 02b5fh                               ; e8 2b fc
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
    and byte [bp+020h], 0feh                  ; 80 66 20 fe
    mov dx, 00441h                            ; ba 41 04
    xor ax, ax                                ; 31 c0
    call 01600h                               ; e8 bc e6
    movzx dx, al                              ; 0f b6 d0
    sal dx, 008h                              ; c1 e2 08
    or si, dx                                 ; 09 d6
    mov word [bp+01ah], si                    ; 89 76 1a
    test al, al                               ; 84 c0
    je short 02f34h                           ; 74 e1
    jmp near 032b1h                           ; e9 5b 03
    mov al, byte [bp+01ah]                    ; 8a 46 1a
    mov byte [bp-008h], al                    ; 88 46 f8
    mov dx, word [bp+018h]                    ; 8b 56 18
    shr dx, 008h                              ; c1 ea 08
    mov byte [bp-004h], dl                    ; 88 56 fc
    mov al, byte [bp+018h]                    ; 8a 46 18
    mov byte [bp-006h], al                    ; 88 46 fa
    mov dx, word [bp+016h]                    ; 8b 56 16
    shr dx, 008h                              ; c1 ea 08
    mov byte [bp-002h], dl                    ; 88 56 fe
    mov byte [bp-00ah], ah                    ; 88 66 f6
    cmp ah, 001h                              ; 80 fc 01
    jnbe short 02f8ch                         ; 77 10
    cmp dl, 001h                              ; 80 fa 01
    jnbe short 02f8ch                         ; 77 0b
    mov al, byte [bp-008h]                    ; 8a 46 f8
    test al, al                               ; 84 c0
    je short 02f8ch                           ; 74 04
    cmp AL, strict byte 048h                  ; 3c 48
    jbe short 02fb5h                          ; 76 29
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 17 e9
    push 001e5h                               ; 68 e5 01
    push 001fdh                               ; 68 fd 01
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 47 e9
    add sp, strict byte 00006h                ; 83 c4 06
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    xor ah, ah                                ; 30 e4
    or ah, 001h                               ; 80 cc 01
    mov word [bp+01ah], ax                    ; 89 46 1a
    mov ax, strict word 00001h                ; b8 01 00
    jmp short 03026h                          ; eb 71
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6
    call 02e1eh                               ; e8 62 fe
    test ax, ax                               ; 85 c0
    je near 030bfh                            ; 0f 84 fd 00
    movzx dx, byte [bp-00ah]                  ; 0f b6 56 f6
    mov ax, dx                                ; 89 d0
    call 02c3dh                               ; e8 72 fc
    test ax, ax                               ; 85 c0
    jne short 02fe8h                          ; 75 19
    mov ax, dx                                ; 89 d0
    call 02d24h                               ; e8 50 fd
    test ax, ax                               ; 85 c0
    jne short 02fe8h                          ; 75 10
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    xor ah, ah                                ; 30 e4
    or ah, 00ch                               ; 80 cc 0c
    mov word [bp+01ah], ax                    ; 89 46 1a
    mov ax, strict word 0000ch                ; b8 0c 00
    jmp short 03026h                          ; eb 3e
    cmp ch, 002h                              ; 80 fd 02
    jne near 0317dh                           ; 0f 85 8e 01
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    shr dx, 00ch                              ; c1 ea 0c
    mov ah, dl                                ; 88 d4
    mov cx, word [bp+00ah]                    ; 8b 4e 0a
    sal cx, 004h                              ; c1 e1 04
    mov bx, word [bp+014h]                    ; 8b 5e 14
    add bx, cx                                ; 01 cb
    cmp bx, cx                                ; 39 cb
    jnc short 03008h                          ; 73 02
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4
    movzx cx, byte [bp-008h]                  ; 0f b6 4e f8
    sal cx, 009h                              ; c1 e1 09
    dec cx                                    ; 49
    mov dx, bx                                ; 89 da
    add dx, cx                                ; 01 ca
    cmp dx, bx                                ; 39 da
    jnc short 03030h                          ; 73 18
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    xor ah, ah                                ; 30 e4
    or ah, 009h                               ; 80 cc 09
    mov word [bp+01ah], ax                    ; 89 46 1a
    mov ax, strict word 00009h                ; b8 09 00
    call 02b4ah                               ; e8 21 fb
    mov byte [bp+01ah], 000h                  ; c6 46 1a 00
    jmp near 032b1h                           ; e9 81 02
    mov AL, strict byte 006h                  ; b0 06
    mov dx, strict word 0000ah                ; ba 0a 00
    out DX, AL                                ; ee
    xor al, al                                ; 30 c0
    mov dx, strict word 0000ch                ; ba 0c 00
    out DX, AL                                ; ee
    mov al, bl                                ; 88 d8
    mov dx, strict word 00004h                ; ba 04 00
    out DX, AL                                ; ee
    shr bx, 008h                              ; c1 eb 08
    mov al, bl                                ; 88 d8
    out DX, AL                                ; ee
    xor al, bl                                ; 30 d8
    mov dx, strict word 0000ch                ; ba 0c 00
    out DX, AL                                ; ee
    mov al, cl                                ; 88 c8
    mov dx, strict word 00005h                ; ba 05 00
    out DX, AL                                ; ee
    shr cx, 008h                              ; c1 e9 08
    mov al, cl                                ; 88 c8
    out DX, AL                                ; ee
    mov AL, strict byte 046h                  ; b0 46
    mov dx, strict word 0000bh                ; ba 0b 00
    out DX, AL                                ; ee
    mov al, ah                                ; 88 e0
    mov dx, 00081h                            ; ba 81 00
    out DX, AL                                ; ee
    mov AL, strict byte 002h                  ; b0 02
    mov dx, strict word 0000ah                ; ba 0a 00
    out DX, AL                                ; ee
    out DX, AL                                ; ee
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6
    call 02bb1h                               ; e8 3d fb
    mov AL, strict byte 0e6h                  ; b0 e6
    mov dx, 003f5h                            ; ba f5 03
    out DX, AL                                ; ee
    movzx dx, byte [bp-002h]                  ; 0f b6 56 fe
    sal dx, 002h                              ; c1 e2 02
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6
    or ax, dx                                 ; 09 d0
    mov dx, 003f5h                            ; ba f5 03
    out DX, AL                                ; ee
    mov al, byte [bp-004h]                    ; 8a 46 fc
    out DX, AL                                ; ee
    mov al, byte [bp-002h]                    ; 8a 46 fe
    out DX, AL                                ; ee
    mov al, byte [bp-006h]                    ; 8a 46 fa
    out DX, AL                                ; ee
    mov AL, strict byte 002h                  ; b0 02
    out DX, AL                                ; ee
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8
    add ax, dx                                ; 01 d0
    dec ax                                    ; 48
    mov dx, 003f5h                            ; ba f5 03
    out DX, AL                                ; ee
    xor al, al                                ; 30 c0
    out DX, AL                                ; ee
    mov AL, strict byte 0ffh                  ; b0 ff
    out DX, AL                                ; ee
    sti                                       ; fb
    mov dx, strict word 00040h                ; ba 40 00
    mov ax, dx                                ; 89 d0
    call 01600h                               ; e8 48 e5
    test al, al                               ; 84 c0
    jne short 030d0h                          ; 75 14
    call 02b89h                               ; e8 ca fa
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    xor ah, ah                                ; 30 e4
    or ah, 080h                               ; 80 cc 80
    mov word [bp+01ah], ax                    ; 89 46 1a
    mov ax, 00080h                            ; b8 80 00
    jmp near 03026h                           ; e9 56 ff
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 27 e5
    and AL, strict byte 080h                  ; 24 80
    test al, al                               ; 84 c0
    je short 030b0h                           ; 74 d1
    cli                                       ; fa
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 17 e5
    and AL, strict byte 07fh                  ; 24 7f
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 17 e5
    mov dx, 003f4h                            ; ba f4 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and AL, strict byte 0c0h                  ; 24 c0
    cmp AL, strict byte 0c0h                  ; 3c c0
    je short 03111h                           ; 74 0e
    push 001e5h                               ; 68 e5 01
    push 00218h                               ; 68 18 02
    push strict byte 00007h                   ; 6a 07
    call 018e9h                               ; e8 db e7
    add sp, strict byte 00006h                ; 83 c4 06
    xor si, si                                ; 31 f6
    jmp short 0311ah                          ; eb 05
    cmp si, strict byte 00007h                ; 83 fe 07
    jnl short 03132h                          ; 7d 18
    mov dx, 003f5h                            ; ba f5 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov byte [bp+si-012h], al                 ; 88 42 ee
    movzx bx, al                              ; 0f b6 d8
    lea dx, [si+042h]                         ; 8d 54 42
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 df e4
    inc si                                    ; 46
    jmp short 03115h                          ; eb e3
    test byte [bp-012h], 0c0h                 ; f6 46 ee c0
    je short 03149h                           ; 74 11
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    xor ah, ah                                ; 30 e4
    or ah, 020h                               ; 80 cc 20
    mov word [bp+01ah], ax                    ; 89 46 1a
    mov ax, strict word 00020h                ; b8 20 00
    jmp near 03026h                           ; e9 dd fe
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    sal ax, 009h                              ; c1 e0 09
    cwd                                       ; 99
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2
    sar ax, 1                                 ; d1 f8
    mov cx, ax                                ; 89 c1
    mov si, word [bp+014h]                    ; 8b 76 14
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    mov di, si                                ; 89 f7
    mov es, dx                                ; 8e c2
    push DS                                   ; 1e
    mov ds, dx                                ; 8e da
    rep movsw                                 ; f3 a5
    pop DS                                    ; 1f
    movzx dx, byte [bp-004h]                  ; 0f b6 56 fc
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6
    call 02b5fh                               ; e8 ed f9
    mov byte [bp+01bh], 000h                  ; c6 46 1b 00
    and byte [bp+020h], 0feh                  ; 80 66 20 fe
    jmp near 02f34h                           ; e9 b7 fd
    cmp ch, 003h                              ; 80 fd 03
    jne near 032bfh                           ; 0f 85 3b 01
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    shr dx, 00ch                              ; c1 ea 0c
    mov ah, dl                                ; 88 d4
    mov cx, word [bp+00ah]                    ; 8b 4e 0a
    sal cx, 004h                              ; c1 e1 04
    mov bx, word [bp+014h]                    ; 8b 5e 14
    add bx, cx                                ; 01 cb
    cmp bx, cx                                ; 39 cb
    jnc short 0319dh                          ; 73 02
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4
    movzx cx, byte [bp-008h]                  ; 0f b6 4e f8
    sal cx, 009h                              ; c1 e1 09
    dec cx                                    ; 49
    mov dx, bx                                ; 89 da
    add dx, cx                                ; 01 ca
    cmp dx, bx                                ; 39 da
    jc near 03018h                            ; 0f 82 69 fe
    mov AL, strict byte 006h                  ; b0 06
    mov dx, strict word 0000ah                ; ba 0a 00
    out DX, AL                                ; ee
    xor al, al                                ; 30 c0
    mov dx, strict word 0000ch                ; ba 0c 00
    out DX, AL                                ; ee
    mov al, bl                                ; 88 d8
    mov dx, strict word 00004h                ; ba 04 00
    out DX, AL                                ; ee
    shr bx, 008h                              ; c1 eb 08
    mov al, bl                                ; 88 d8
    out DX, AL                                ; ee
    xor al, bl                                ; 30 d8
    mov dx, strict word 0000ch                ; ba 0c 00
    out DX, AL                                ; ee
    mov al, cl                                ; 88 c8
    mov dx, strict word 00005h                ; ba 05 00
    out DX, AL                                ; ee
    shr cx, 008h                              ; c1 e9 08
    mov al, cl                                ; 88 c8
    out DX, AL                                ; ee
    mov AL, strict byte 04ah                  ; b0 4a
    mov dx, strict word 0000bh                ; ba 0b 00
    out DX, AL                                ; ee
    mov al, ah                                ; 88 e0
    mov dx, 00081h                            ; ba 81 00
    out DX, AL                                ; ee
    mov AL, strict byte 002h                  ; b0 02
    mov dx, strict word 0000ah                ; ba 0a 00
    out DX, AL                                ; ee
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6
    call 02bb1h                               ; e8 bf f9
    mov AL, strict byte 0c5h                  ; b0 c5
    mov dx, 003f5h                            ; ba f5 03
    out DX, AL                                ; ee
    movzx dx, byte [bp-002h]                  ; 0f b6 56 fe
    sal dx, 002h                              ; c1 e2 02
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6
    or ax, dx                                 ; 09 d0
    mov dx, 003f5h                            ; ba f5 03
    out DX, AL                                ; ee
    mov al, byte [bp-004h]                    ; 8a 46 fc
    out DX, AL                                ; ee
    mov al, byte [bp-002h]                    ; 8a 46 fe
    out DX, AL                                ; ee
    mov al, byte [bp-006h]                    ; 8a 46 fa
    out DX, AL                                ; ee
    mov AL, strict byte 002h                  ; b0 02
    out DX, AL                                ; ee
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    add ax, dx                                ; 01 d0
    dec ax                                    ; 48
    mov dx, 003f5h                            ; ba f5 03
    out DX, AL                                ; ee
    xor al, al                                ; 30 c0
    out DX, AL                                ; ee
    mov AL, strict byte 0ffh                  ; b0 ff
    out DX, AL                                ; ee
    sti                                       ; fb
    mov dx, strict word 00040h                ; ba 40 00
    mov ax, dx                                ; 89 d0
    call 01600h                               ; e8 ca e3
    test al, al                               ; 84 c0
    je near 030bch                            ; 0f 84 80 fe
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 bb e3
    and AL, strict byte 080h                  ; 24 80
    test al, al                               ; 84 c0
    je short 0322eh                           ; 74 e3
    cli                                       ; fa
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 ab e3
    and AL, strict byte 07fh                  ; 24 7f
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 ab e3
    mov dx, 003f4h                            ; ba f4 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and AL, strict byte 0c0h                  ; 24 c0
    cmp AL, strict byte 0c0h                  ; 3c c0
    je short 0327dh                           ; 74 0e
    push 001e5h                               ; 68 e5 01
    push 00218h                               ; 68 18 02
    push strict byte 00007h                   ; 6a 07
    call 018e9h                               ; e8 6f e6
    add sp, strict byte 00006h                ; 83 c4 06
    xor si, si                                ; 31 f6
    jmp short 03286h                          ; eb 05
    cmp si, strict byte 00007h                ; 83 fe 07
    jnl short 0329eh                          ; 7d 18
    mov dx, 003f5h                            ; ba f5 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov byte [bp+si-012h], al                 ; 88 42 ee
    movzx bx, al                              ; 0f b6 d8
    lea dx, [si+042h]                         ; 8d 54 42
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 73 e3
    inc si                                    ; 46
    jmp short 03281h                          ; eb e3
    test byte [bp-012h], 0c0h                 ; f6 46 ee c0
    je near 03167h                            ; 0f 84 c1 fe
    test byte [bp-011h], 002h                 ; f6 46 ef 02
    je short 032b8h                           ; 74 0c
    mov word [bp+01ah], 00300h                ; c7 46 1a 00 03
    or byte [bp+020h], 001h                   ; 80 4e 20 01
    jmp near 02f34h                           ; e9 7c fc
    mov word [bp+01ah], 00100h                ; c7 46 1a 00 01
    jmp short 032b1h                          ; eb f2
    movzx dx, byte [bp-004h]                  ; 0f b6 56 fc
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6
    call 02b5fh                               ; e8 95 f8
    and byte [bp+020h], 0feh                  ; 80 66 20 fe
    mov byte [bp+01bh], 000h                  ; c6 46 1b 00
    jmp near 02f34h                           ; e9 5f fc
    mov al, byte [bp+01ah]                    ; 8a 46 1a
    mov byte [bp-008h], al                    ; 88 46 f8
    mov dx, word [bp+018h]                    ; 8b 56 18
    shr dx, 008h                              ; c1 ea 08
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    mov byte [bp-002h], al                    ; 88 46 fe
    mov bl, byte [bp+012h]                    ; 8a 5e 12
    mov byte [bp-00ah], bl                    ; 88 5e f6
    cmp bl, 001h                              ; 80 fb 01
    jnbe short 03309h                         ; 77 14
    cmp AL, strict byte 001h                  ; 3c 01
    jnbe short 03309h                         ; 77 10
    cmp dl, 04fh                              ; 80 fa 4f
    jnbe short 03309h                         ; 77 0b
    mov al, byte [bp-008h]                    ; 8a 46 f8
    test al, al                               ; 84 c0
    je short 03309h                           ; 74 04
    cmp AL, strict byte 012h                  ; 3c 12
    jbe short 0331eh                          ; 76 15
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    xor ah, ah                                ; 30 e4
    or ah, 001h                               ; 80 cc 01
    mov word [bp+01ah], ax                    ; 89 46 1a
    mov ax, strict word 00001h                ; b8 01 00
    call 02b4ah                               ; e8 30 f8
    or byte [bp+020h], 001h                   ; 80 4e 20 01
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6
    call 02e1eh                               ; e8 f9 fa
    test ax, ax                               ; 85 c0
    je near 02f02h                            ; 0f 84 d7 fb
    movzx dx, byte [bp-00ah]                  ; 0f b6 56 f6
    mov ax, dx                                ; 89 d0
    call 02c3dh                               ; e8 09 f9
    test ax, ax                               ; 85 c0
    jne short 03343h                          ; 75 0b
    mov ax, dx                                ; 89 d0
    call 02d24h                               ; e8 e7 f9
    test ax, ax                               ; 85 c0
    je near 02fd8h                            ; 0f 84 95 fc
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    shr dx, 00ch                              ; c1 ea 0c
    mov ah, dl                                ; 88 d4
    mov cx, word [bp+00ah]                    ; 8b 4e 0a
    sal cx, 004h                              ; c1 e1 04
    mov bx, word [bp+014h]                    ; 8b 5e 14
    add bx, cx                                ; 01 cb
    cmp bx, cx                                ; 39 cb
    jnc short 0335ch                          ; 73 02
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4
    movzx cx, byte [bp-008h]                  ; 0f b6 4e f8
    sal cx, 002h                              ; c1 e1 02
    dec cx                                    ; 49
    mov dx, bx                                ; 89 da
    add dx, cx                                ; 01 ca
    cmp dx, bx                                ; 39 da
    jc near 03018h                            ; 0f 82 aa fc
    mov AL, strict byte 006h                  ; b0 06
    mov dx, strict word 0000ah                ; ba 0a 00
    out DX, AL                                ; ee
    xor al, al                                ; 30 c0
    mov dx, strict word 0000ch                ; ba 0c 00
    out DX, AL                                ; ee
    mov al, bl                                ; 88 d8
    mov dx, strict word 00004h                ; ba 04 00
    out DX, AL                                ; ee
    shr bx, 008h                              ; c1 eb 08
    mov al, bl                                ; 88 d8
    out DX, AL                                ; ee
    xor al, bl                                ; 30 d8
    mov dx, strict word 0000ch                ; ba 0c 00
    out DX, AL                                ; ee
    mov al, cl                                ; 88 c8
    mov dx, strict word 00005h                ; ba 05 00
    out DX, AL                                ; ee
    shr cx, 008h                              ; c1 e9 08
    mov al, cl                                ; 88 c8
    out DX, AL                                ; ee
    mov AL, strict byte 04ah                  ; b0 4a
    mov dx, strict word 0000bh                ; ba 0b 00
    out DX, AL                                ; ee
    mov al, ah                                ; 88 e0
    mov dx, 00081h                            ; ba 81 00
    out DX, AL                                ; ee
    mov AL, strict byte 002h                  ; b0 02
    mov dx, strict word 0000ah                ; ba 0a 00
    out DX, AL                                ; ee
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6
    call 02bb1h                               ; e8 00 f8
    mov AL, strict byte 04dh                  ; b0 4d
    mov dx, 003f5h                            ; ba f5 03
    out DX, AL                                ; ee
    movzx dx, byte [bp-002h]                  ; 0f b6 56 fe
    sal dx, 002h                              ; c1 e2 02
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6
    or ax, dx                                 ; 09 d0
    mov dx, 003f5h                            ; ba f5 03
    out DX, AL                                ; ee
    mov AL, strict byte 002h                  ; b0 02
    out DX, AL                                ; ee
    mov al, byte [bp-008h]                    ; 8a 46 f8
    out DX, AL                                ; ee
    xor al, al                                ; 30 c0
    out DX, AL                                ; ee
    mov AL, strict byte 0f6h                  ; b0 f6
    out DX, AL                                ; ee
    sti                                       ; fb
    mov dx, strict word 00040h                ; ba 40 00
    mov ax, dx                                ; 89 d0
    call 01600h                               ; e8 22 e2
    test al, al                               ; 84 c0
    jne short 033e8h                          ; 75 06
    call 02b89h                               ; e8 a4 f7
    jmp near 02f02h                           ; e9 1a fb
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 0f e2
    and AL, strict byte 080h                  ; 24 80
    test al, al                               ; 84 c0
    je short 033d6h                           ; 74 df
    cli                                       ; fa
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 ff e1
    and AL, strict byte 07fh                  ; 24 7f
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 ff e1
    mov dx, 003f4h                            ; ba f4 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and AL, strict byte 0c0h                  ; 24 c0
    cmp AL, strict byte 0c0h                  ; 3c c0
    je short 03429h                           ; 74 0e
    push 001e5h                               ; 68 e5 01
    push 00218h                               ; 68 18 02
    push strict byte 00007h                   ; 6a 07
    call 018e9h                               ; e8 c3 e4
    add sp, strict byte 00006h                ; 83 c4 06
    xor si, si                                ; 31 f6
    jmp short 03432h                          ; eb 05
    cmp si, strict byte 00007h                ; 83 fe 07
    jnl short 0344ah                          ; 7d 18
    mov dx, 003f5h                            ; ba f5 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov byte [bp+si-012h], al                 ; 88 42 ee
    movzx bx, al                              ; 0f b6 d8
    lea dx, [si+042h]                         ; 8d 54 42
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 c7 e1
    inc si                                    ; 46
    jmp short 0342dh                          ; eb e3
    test byte [bp-012h], 0c0h                 ; f6 46 ee c0
    je short 03466h                           ; 74 16
    test byte [bp-011h], 002h                 ; f6 46 ef 02
    jne near 032ach                           ; 0f 85 54 fe
    push 001e5h                               ; 68 e5 01
    push 0022ch                               ; 68 2c 02
    push strict byte 00007h                   ; 6a 07
    call 018e9h                               ; e8 86 e4
    add sp, strict byte 00006h                ; 83 c4 06
    xor al, al                                ; 30 c0
    mov byte [bp+01bh], al                    ; 88 46 1b
    xor ah, ah                                ; 30 e4
    call 02b4ah                               ; e8 da f6
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6
    xor dx, dx                                ; 31 d2
    call 02b5fh                               ; e8 e6 f6
    jmp near 03176h                           ; e9 fa fc
    mov byte [bp-00ah], ah                    ; 88 66 f6
    cmp ah, 001h                              ; 80 fc 01
    jbe short 034a4h                          ; 76 20
    xor ax, ax                                ; 31 c0
    mov word [bp+01ah], ax                    ; 89 46 1a
    mov word [bp+014h], ax                    ; 89 46 14
    mov word [bp+018h], ax                    ; 89 46 18
    mov word [bp+016h], ax                    ; 89 46 16
    mov word [bp+00ah], ax                    ; 89 46 0a
    mov word [bp+00ch], ax                    ; 89 46 0c
    movzx ax, cl                              ; 0f b6 c1
    mov word [bp+016h], ax                    ; 89 46 16
    mov word [bp+020h], dx                    ; 89 56 20
    jmp near 02f34h                           ; e9 90 fa
    mov ax, strict word 00010h                ; b8 10 00
    call 0165ch                               ; e8 b2 e1
    mov bl, al                                ; 88 c3
    xor cl, cl                                ; 30 c9
    test AL, strict byte 0f0h                 ; a8 f0
    je short 034b4h                           ; 74 02
    mov CL, strict byte 001h                  ; b1 01
    test bl, 00fh                             ; f6 c3 0f
    je short 034bbh                           ; 74 02
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1
    cmp byte [bp-00ah], 000h                  ; 80 7e f6 00
    jne short 034c6h                          ; 75 05
    shr bl, 004h                              ; c0 eb 04
    jmp short 034c9h                          ; eb 03
    and bl, 00fh                              ; 80 e3 0f
    xor al, al                                ; 30 c0
    mov byte [bp+015h], al                    ; 88 46 15
    movzx si, bl                              ; 0f b6 f3
    mov word [bp+014h], si                    ; 89 76 14
    xor ah, ah                                ; 30 e4
    mov word [bp+01ah], ax                    ; 89 46 1a
    mov dx, word [bp+016h]                    ; 8b 56 16
    xor dl, dl                                ; 30 d2
    movzx ax, cl                              ; 0f b6 c1
    or dx, ax                                 ; 09 c2
    mov word [bp+016h], dx                    ; 89 56 16
    cmp bl, 008h                              ; 80 fb 08
    jnbe short 03544h                         ; 77 59
    add si, si                                ; 01 f6
    mov ax, dx                                ; 89 d0
    xor ah, dh                                ; 30 f4
    mov bx, ax                                ; 89 c3
    or bh, 001h                               ; 80 cf 01
    jmp word [cs:si+02e3fh]                   ; 2e ff a4 3f 2e
    mov word [bp+018h], strict word 00000h    ; c7 46 18 00 00
    mov byte [bp+017h], 000h                  ; c6 46 17 00
    jmp short 03552h                          ; eb 4c
    mov word [bp+018h], 02709h                ; c7 46 18 09 27
    mov word [bp+016h], bx                    ; 89 5e 16
    jmp short 03552h                          ; eb 42
    mov word [bp+018h], 04f0fh                ; c7 46 18 0f 4f
    jmp short 0350bh                          ; eb f4
    mov word [bp+018h], 04f09h                ; c7 46 18 09 4f
    jmp short 0350bh                          ; eb ed
    mov word [bp+018h], 04f12h                ; c7 46 18 12 4f
    jmp short 0350bh                          ; eb e6
    mov word [bp+018h], 04f24h                ; c7 46 18 24 4f
    jmp short 0350bh                          ; eb df
    mov word [bp+018h], 02708h                ; c7 46 18 08 27
    mov word [bp+016h], ax                    ; 89 46 16
    jmp short 03552h                          ; eb 1c
    mov word [bp+018h], 02709h                ; c7 46 18 09 27
    jmp short 03531h                          ; eb f4
    mov word [bp+018h], 02708h                ; c7 46 18 08 27
    jmp short 0350bh                          ; eb c7
    push 001e5h                               ; 68 e5 01
    push 0023dh                               ; 68 3d 02
    push strict byte 00007h                   ; 6a 07
    call 018e9h                               ; e8 9a e3
    add sp, strict byte 00006h                ; 83 c4 06
    mov word [bp+00ah], 0f000h                ; c7 46 0a 00 f0
    mov word [bp+00ch], 0efc7h                ; c7 46 0c c7 ef
    jmp near 03176h                           ; e9 17 fc
    mov byte [bp-00ah], ah                    ; 88 66 f6
    cmp ah, 001h                              ; 80 fc 01
    jbe short 0356dh                          ; 76 06
    mov word [bp+01ah], si                    ; 89 76 1a
    jmp near 0349eh                           ; e9 31 ff
    mov ax, strict word 00010h                ; b8 10 00
    call 0165ch                               ; e8 e9 e0
    cmp byte [bp-00ah], 000h                  ; 80 7e f6 00
    jne short 03580h                          ; 75 07
    mov bl, al                                ; 88 c3
    shr bl, 004h                              ; c0 eb 04
    jmp short 03585h                          ; eb 05
    mov bl, al                                ; 88 c3
    and bl, 00fh                              ; 80 e3 0f
    and byte [bp+020h], 0feh                  ; 80 66 20 fe
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    xor ah, ah                                ; 30 e4
    test bl, bl                               ; 84 db
    je short 03595h                           ; 74 03
    or ah, 001h                               ; 80 cc 01
    mov word [bp+01ah], ax                    ; 89 46 1a
    jmp near 02f34h                           ; e9 99 f9
    cmp ah, 001h                              ; 80 fc 01
    jbe short 035ach                          ; 76 0c
    mov word [bp+01ah], si                    ; 89 76 1a
    mov ax, strict word 00001h                ; b8 01 00
    call 02b4ah                               ; e8 a1 f5
    jmp near 0349eh                           ; e9 f2 fe
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    xor ah, ah                                ; 30 e4
    or ah, 006h                               ; 80 cc 06
    mov word [bp+01ah], ax                    ; 89 46 1a
    mov ax, strict word 00006h                ; b8 06 00
    jmp near 02ee0h                           ; e9 23 f9
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 e6 e2
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 001e5h                               ; 68 e5 01
    push 00252h                               ; 68 52 02
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 0f e3
    add sp, strict byte 00008h                ; 83 c4 08
    jmp near 02ed2h                           ; e9 f2 f8
_cdemu_init:                                 ; 0xf35e0 LB 0x16
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 30 e0
    xor bx, bx                                ; 31 db
    mov dx, 00322h                            ; ba 22 03
    call 0160eh                               ; e8 1a e0
    pop bp                                    ; 5d
    retn                                      ; c3
_cdemu_isactive:                             ; 0xf35f6 LB 0x14
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 1a e0
    mov dx, 00322h                            ; ba 22 03
    call 01600h                               ; e8 f8 df
    pop bp                                    ; 5d
    retn                                      ; c3
_cdemu_emulated_drive:                       ; 0xf360a LB 0x14
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 06 e0
    mov dx, 00324h                            ; ba 24 03
    call 01600h                               ; e8 e4 df
    pop bp                                    ; 5d
    retn                                      ; c3
_int13_eltorito:                             ; 0xf361e LB 0x186
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 f0 df
    mov si, 00322h                            ; be 22 03
    mov di, ax                                ; 89 c7
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    shr ax, 008h                              ; c1 e8 08
    cmp ax, strict word 0004bh                ; 3d 4b 00
    jc short 03646h                           ; 72 0a
    jbe short 0366ch                          ; 76 2e
    cmp ax, strict word 0004dh                ; 3d 4d 00
    jbe short 0364dh                          ; 76 0a
    jmp near 03768h                           ; e9 22 01
    cmp ax, strict word 0004ah                ; 3d 4a 00
    jne near 03768h                           ; 0f 85 1b 01
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 56 e2
    push word [bp+01ah]                       ; ff 76 1a
    push 0026ch                               ; 68 6c 02
    push 0027bh                               ; 68 7b 02
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 83 e2
    add sp, strict byte 00008h                ; 83 c4 08
    jmp near 03783h                           ; e9 17 01
    mov dx, word [bp+00eh]                    ; 8b 56 0e
    mov ax, word [bp+008h]                    ; 8b 46 08
    mov bx, strict word 00013h                ; bb 13 00
    call 0160eh                               ; e8 96 df
    mov es, di                                ; 8e c7
    movzx bx, byte [es:si+001h]               ; 26 0f b6 5c 01
    mov dx, word [bp+00eh]                    ; 8b 56 0e
    inc dx                                    ; 42
    mov ax, word [bp+008h]                    ; 8b 46 08
    call 0160eh                               ; e8 85 df
    mov es, di                                ; 8e c7
    movzx bx, byte [es:si+002h]               ; 26 0f b6 5c 02
    mov dx, word [bp+00eh]                    ; 8b 56 0e
    inc dx                                    ; 42
    inc dx                                    ; 42
    mov ax, word [bp+008h]                    ; 8b 46 08
    call 0160eh                               ; e8 73 df
    mov es, di                                ; 8e c7
    movzx bx, byte [es:si+003h]               ; 26 0f b6 5c 03
    mov dx, word [bp+00eh]                    ; 8b 56 0e
    add dx, strict byte 00003h                ; 83 c2 03
    mov ax, word [bp+008h]                    ; 8b 46 08
    call 0160eh                               ; e8 60 df
    mov es, di                                ; 8e c7
    mov bx, word [es:si+008h]                 ; 26 8b 5c 08
    mov cx, word [es:si+00ah]                 ; 26 8b 4c 0a
    mov dx, word [bp+00eh]                    ; 8b 56 0e
    add dx, strict byte 00004h                ; 83 c2 04
    mov ax, word [bp+008h]                    ; 8b 46 08
    call 0164ah                               ; e8 86 df
    mov es, di                                ; 8e c7
    mov bx, word [es:si+004h]                 ; 26 8b 5c 04
    mov dx, word [bp+00eh]                    ; 8b 56 0e
    add dx, strict byte 00008h                ; 83 c2 08
    mov ax, word [bp+008h]                    ; 8b 46 08
    call 0162ah                               ; e8 54 df
    mov es, di                                ; 8e c7
    mov bx, word [es:si+006h]                 ; 26 8b 5c 06
    mov dx, word [bp+00eh]                    ; 8b 56 0e
    add dx, strict byte 0000ah                ; 83 c2 0a
    mov ax, word [bp+008h]                    ; 8b 46 08
    call 0162ah                               ; e8 42 df
    mov es, di                                ; 8e c7
    mov bx, word [es:si+00ch]                 ; 26 8b 5c 0c
    mov dx, word [bp+00eh]                    ; 8b 56 0e
    add dx, strict byte 0000ch                ; 83 c2 0c
    mov ax, word [bp+008h]                    ; 8b 46 08
    call 0162ah                               ; e8 30 df
    mov es, di                                ; 8e c7
    mov bx, word [es:si+00eh]                 ; 26 8b 5c 0e
    mov dx, word [bp+00eh]                    ; 8b 56 0e
    add dx, strict byte 0000eh                ; 83 c2 0e
    mov ax, word [bp+008h]                    ; 8b 46 08
    call 0162ah                               ; e8 1e df
    mov es, di                                ; 8e c7
    movzx bx, byte [es:si+012h]               ; 26 0f b6 5c 12
    mov dx, word [bp+00eh]                    ; 8b 56 0e
    add dx, strict byte 00010h                ; 83 c2 10
    mov ax, word [bp+008h]                    ; 8b 46 08
    call 0160eh                               ; e8 ef de
    mov es, di                                ; 8e c7
    movzx bx, byte [es:si+014h]               ; 26 0f b6 5c 14
    mov dx, word [bp+00eh]                    ; 8b 56 0e
    add dx, strict byte 00011h                ; 83 c2 11
    mov ax, word [bp+008h]                    ; 8b 46 08
    call 0160eh                               ; e8 dc de
    mov es, di                                ; 8e c7
    movzx bx, byte [es:si+010h]               ; 26 0f b6 5c 10
    mov dx, word [bp+00eh]                    ; 8b 56 0e
    add dx, strict byte 00012h                ; 83 c2 12
    mov ax, word [bp+008h]                    ; 8b 46 08
    call 0160eh                               ; e8 c9 de
    test byte [bp+01ah], 0ffh                 ; f6 46 1a ff
    jne short 03751h                          ; 75 06
    mov es, di                                ; 8e c7
    mov byte [es:si], 000h                    ; 26 c6 04 00
    mov byte [bp+01bh], 000h                  ; c6 46 1b 00
    xor bx, bx                                ; 31 db
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 ae de
    and byte [bp+020h], 0feh                  ; 80 66 20 fe
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 3b e1
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 0026ch                               ; 68 6c 02
    push 002a3h                               ; 68 a3 02
    jmp near 03661h                           ; e9 de fe
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    xor ah, ah                                ; 30 e4
    or ah, 001h                               ; 80 cc 01
    mov word [bp+01ah], ax                    ; 89 46 1a
    mov bx, ax                                ; 89 c3
    shr bx, 008h                              ; c1 eb 08
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 70 de
    or byte [bp+020h], 001h                   ; 80 4e 20 01
    jmp short 03764h                          ; eb c0
device_is_cdrom_:                            ; 0xf37a4 LB 0x32
    push bx                                   ; 53
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bl, al                                ; 88 c3
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 68 de
    cmp bl, 010h                              ; 80 fb 10
    jc short 037bdh                           ; 72 04
    xor ax, ax                                ; 31 c0
    jmp short 037d2h                          ; eb 15
    xor bh, bh                                ; 30 ff
    imul bx, bx, strict byte 00018h           ; 6b db 18
    mov es, ax                                ; 8e c0
    add bx, 00122h                            ; 81 c3 22 01
    cmp byte [es:bx+01fh], 005h               ; 26 80 7f 1f 05
    jne short 037b9h                          ; 75 ea
    mov ax, strict word 00001h                ; b8 01 00
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    retn                                      ; c3
cdrom_boot_:                                 ; 0xf37d6 LB 0x410
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push si                                   ; 56
    push di                                   ; 57
    enter 0081ch, 000h                        ; c8 1c 08 00
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 34 de
    mov word [bp-00eh], ax                    ; 89 46 f2
    mov si, 00322h                            ; be 22 03
    mov word [bp-006h], ax                    ; 89 46 fa
    mov word [bp-00ah], 00122h                ; c7 46 f6 22 01
    mov word [bp-008h], ax                    ; 89 46 f8
    mov byte [bp-002h], 000h                  ; c6 46 fe 00
    jmp short 03808h                          ; eb 09
    inc byte [bp-002h]                        ; fe 46 fe
    cmp byte [bp-002h], 010h                  ; 80 7e fe 10
    jnc short 03813h                          ; 73 0b
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    call 037a4h                               ; e8 95 ff
    test ax, ax                               ; 85 c0
    je short 037ffh                           ; 74 ec
    cmp byte [bp-002h], 010h                  ; 80 7e fe 10
    jc short 0381fh                           ; 72 06
    mov ax, strict word 00002h                ; b8 02 00
    jmp near 03b86h                           ; e9 67 03
    mov cx, strict word 0000ch                ; b9 0c 00
    xor bx, bx                                ; 31 db
    mov dx, ss                                ; 8c d2
    lea ax, [bp-01ch]                         ; 8d 46 e4
    call 0901ah                               ; e8 ee 57
    mov word [bp-01ch], strict word 00028h    ; c7 46 e4 28 00
    mov ax, strict word 00011h                ; b8 11 00
    xor dx, dx                                ; 31 d2
    xchg ah, al                               ; 86 c4
    xchg dh, dl                               ; 86 d6
    xchg dx, ax                               ; 92
    mov word [bp-01ah], ax                    ; 89 46 e6
    mov word [bp-018h], dx                    ; 89 56 e8
    mov ax, strict word 00001h                ; b8 01 00
    xchg ah, al                               ; 86 c4
    mov word [bp-015h], ax                    ; 89 46 eb
    les bx, [bp-00ah]                         ; c4 5e f6
    db  066h, 026h, 0c7h, 047h, 00ah, 001h, 000h, 000h, 008h
    ; mov dword [es:bx+00ah], strict dword 008000001h ; 66 26 c7 47 0a 01 00 00 08
    mov byte [bp-004h], 000h                  ; c6 46 fc 00
    jmp short 03864h                          ; eb 09
    inc byte [bp-004h]                        ; fe 46 fc
    cmp byte [bp-004h], 004h                  ; 80 7e fc 04
    jnbe short 0389bh                         ; 77 37
    movzx di, byte [bp-002h]                  ; 0f b6 7e fe
    imul di, di, strict byte 00018h           ; 6b ff 18
    mov es, [bp-008h]                         ; 8e 46 f8
    add di, word [bp-00ah]                    ; 03 7e f6
    movzx di, byte [es:di+01eh]               ; 26 0f b6 7d 1e
    add di, di                                ; 01 ff
    lea dx, [bp-0081ch]                       ; 8d 96 e4 f7
    push SS                                   ; 16
    push dx                                   ; 52
    push strict byte 00001h                   ; 6a 01
    push strict byte 00000h                   ; 6a 00
    push 00800h                               ; 68 00 08
    push strict byte 00000h                   ; 6a 00
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    mov cx, ss                                ; 8c d1
    lea bx, [bp-01ch]                         ; 8d 5e e4
    mov dx, strict word 0000ch                ; ba 0c 00
    call word [word di+00000h]                ; ff 95 00 00
    test ax, ax                               ; 85 c0
    jne short 0385bh                          ; 75 c0
    test ax, ax                               ; 85 c0
    je short 038a5h                           ; 74 06
    mov ax, strict word 00003h                ; b8 03 00
    jmp near 03b86h                           ; e9 e1 02
    cmp byte [bp-0081ch], 000h                ; 80 be e4 f7 00
    je short 038b2h                           ; 74 06
    mov ax, strict word 00004h                ; b8 04 00
    jmp near 03b86h                           ; e9 d4 02
    xor di, di                                ; 31 ff
    jmp short 038bch                          ; eb 06
    inc di                                    ; 47
    cmp di, strict byte 00005h                ; 83 ff 05
    jnc short 038cch                          ; 73 10
    mov al, byte [bp+di-0081bh]               ; 8a 83 e5 f7
    cmp al, byte [di+00d3ah]                  ; 3a 85 3a 0d
    je short 038b6h                           ; 74 f0
    mov ax, strict word 00005h                ; b8 05 00
    jmp near 03b86h                           ; e9 ba 02
    xor di, di                                ; 31 ff
    jmp short 038d6h                          ; eb 06
    inc di                                    ; 47
    cmp di, strict byte 00017h                ; 83 ff 17
    jnc short 038e6h                          ; 73 10
    mov al, byte [bp+di-00815h]               ; 8a 83 eb f7
    cmp al, byte [di+00d40h]                  ; 3a 85 40 0d
    je short 038d0h                           ; 74 f0
    mov ax, strict word 00006h                ; b8 06 00
    jmp near 03b86h                           ; e9 a0 02
    mov ax, word [bp-007d5h]                  ; 8b 86 2b f8
    mov dx, word [bp-007d3h]                  ; 8b 96 2d f8
    mov word [bp-01ch], strict word 00028h    ; c7 46 e4 28 00
    xchg ah, al                               ; 86 c4
    xchg dh, dl                               ; 86 d6
    xchg dx, ax                               ; 92
    mov word [bp-01ah], ax                    ; 89 46 e6
    mov word [bp-018h], dx                    ; 89 56 e8
    mov ax, strict word 00001h                ; b8 01 00
    xchg ah, al                               ; 86 c4
    mov word [bp-015h], ax                    ; 89 46 eb
    movzx di, byte [bp-002h]                  ; 0f b6 7e fe
    imul di, di, strict byte 00018h           ; 6b ff 18
    mov es, [bp-008h]                         ; 8e 46 f8
    add di, word [bp-00ah]                    ; 03 7e f6
    movzx di, byte [es:di+01eh]               ; 26 0f b6 7d 1e
    add di, di                                ; 01 ff
    lea dx, [bp-0081ch]                       ; 8d 96 e4 f7
    push SS                                   ; 16
    push dx                                   ; 52
    push strict byte 00001h                   ; 6a 01
    push strict byte 00000h                   ; 6a 00
    push 00800h                               ; 68 00 08
    push strict byte 00000h                   ; 6a 00
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    mov cx, ss                                ; 8c d1
    lea bx, [bp-01ch]                         ; 8d 5e e4
    mov dx, strict word 0000ch                ; ba 0c 00
    call word [word di+00000h]                ; ff 95 00 00
    test ax, ax                               ; 85 c0
    je short 03943h                           ; 74 06
    mov ax, strict word 00007h                ; b8 07 00
    jmp near 03b86h                           ; e9 43 02
    cmp byte [bp-0081ch], 001h                ; 80 be e4 f7 01
    je short 03950h                           ; 74 06
    mov ax, strict word 00008h                ; b8 08 00
    jmp near 03b86h                           ; e9 36 02
    cmp byte [bp-0081bh], 000h                ; 80 be e5 f7 00
    je short 0395dh                           ; 74 06
    mov ax, strict word 00009h                ; b8 09 00
    jmp near 03b86h                           ; e9 29 02
    cmp byte [bp-007feh], 055h                ; 80 be 02 f8 55
    je short 0396ah                           ; 74 06
    mov ax, strict word 0000ah                ; b8 0a 00
    jmp near 03b86h                           ; e9 1c 02
    cmp byte [bp-007fdh], 0aah                ; 80 be 03 f8 aa
    jne short 03964h                          ; 75 f3
    cmp byte [bp-007fch], 088h                ; 80 be 04 f8 88
    je short 0397eh                           ; 74 06
    mov ax, strict word 0000bh                ; b8 0b 00
    jmp near 03b86h                           ; e9 08 02
    mov al, byte [bp-007fbh]                  ; 8a 86 05 f8
    mov es, [bp-006h]                         ; 8e 46 fa
    mov byte [es:si+001h], al                 ; 26 88 44 01
    cmp byte [bp-007fbh], 000h                ; 80 be 05 f8 00
    jne short 03997h                          ; 75 07
    mov byte [es:si+002h], 0e0h               ; 26 c6 44 02 e0
    jmp short 039aah                          ; eb 13
    cmp byte [bp-007fbh], 004h                ; 80 be 05 f8 04
    jnc short 039a5h                          ; 73 07
    mov byte [es:si+002h], 000h               ; 26 c6 44 02 00
    jmp short 039aah                          ; eb 05
    mov byte [es:si+002h], 080h               ; 26 c6 44 02 80
    movzx di, byte [bp-002h]                  ; 0f b6 7e fe
    mov ax, di                                ; 89 f8
    cwd                                       ; 99
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2
    sar ax, 1                                 ; d1 f8
    mov es, [bp-006h]                         ; 8e 46 fa
    mov byte [es:si+003h], al                 ; 26 88 44 03
    mov ax, di                                ; 89 f8
    cwd                                       ; 99
    mov bx, strict word 00002h                ; bb 02 00
    idiv bx                                   ; f7 fb
    mov word [es:si+004h], dx                 ; 26 89 54 04
    mov ax, word [bp-007fah]                  ; 8b 86 06 f8
    mov word [bp-00ch], ax                    ; 89 46 f4
    test ax, ax                               ; 85 c0
    jne short 039d8h                          ; 75 05
    mov word [bp-00ch], 007c0h                ; c7 46 f4 c0 07
    mov ax, word [bp-00ch]                    ; 8b 46 f4
    mov es, [bp-006h]                         ; 8e 46 fa
    mov word [es:si+00ch], ax                 ; 26 89 44 0c
    mov word [es:si+006h], strict word 00000h ; 26 c7 44 06 00 00
    mov di, word [bp-007f6h]                  ; 8b be 0a f8
    mov word [es:si+00eh], di                 ; 26 89 7c 0e
    test di, di                               ; 85 ff
    je short 039fah                           ; 74 06
    cmp di, 00400h                            ; 81 ff 00 04
    jbe short 03a00h                          ; 76 06
    mov ax, strict word 0000ch                ; b8 0c 00
    jmp near 03b86h                           ; e9 86 01
    mov ax, word [bp-007f4h]                  ; 8b 86 0c f8
    mov dx, word [bp-007f2h]                  ; 8b 96 0e f8
    mov word [es:si+008h], ax                 ; 26 89 44 08
    mov word [es:si+00ah], dx                 ; 26 89 54 0a
    mov word [bp-01ch], strict word 00028h    ; c7 46 e4 28 00
    xchg ah, al                               ; 86 c4
    xchg dh, dl                               ; 86 d6
    xchg dx, ax                               ; 92
    mov word [bp-01ah], ax                    ; 89 46 e6
    mov word [bp-018h], dx                    ; 89 56 e8
    lea dx, [di-001h]                         ; 8d 55 ff
    shr dx, 002h                              ; c1 ea 02
    inc dx                                    ; 42
    mov ax, dx                                ; 89 d0
    xchg ah, al                               ; 86 c4
    mov word [bp-015h], ax                    ; 89 46 eb
    les bx, [bp-00ah]                         ; c4 5e f6
    mov word [es:bx+00ah], dx                 ; 26 89 57 0a
    mov word [es:bx+00ch], 00200h             ; 26 c7 47 0c 00 02
    mov ax, di                                ; 89 f8
    sal ax, 009h                              ; c1 e0 09
    mov dx, 00800h                            ; ba 00 08
    sub dx, ax                                ; 29 c2
    mov ax, dx                                ; 89 d0
    and ah, 007h                              ; 80 e4 07
    mov word [es:bx+01ch], ax                 ; 26 89 47 1c
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    add bx, ax                                ; 01 c3
    movzx ax, byte [es:bx+01eh]               ; 26 0f b6 47 1e
    add ax, ax                                ; 01 c0
    mov word [bp-010h], ax                    ; 89 46 f0
    push word [bp-00ch]                       ; ff 76 f4
    push dword 000000001h                     ; 66 6a 01
    mov ax, di                                ; 89 f8
    xor di, di                                ; 31 ff
    mov cx, strict word 00009h                ; b9 09 00
    sal ax, 1                                 ; d1 e0
    rcl di, 1                                 ; d1 d7
    loop 03a6eh                               ; e2 fa
    push di                                   ; 57
    push ax                                   ; 50
    push strict byte 00000h                   ; 6a 00
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    mov cx, ss                                ; 8c d1
    lea bx, [bp-01ch]                         ; 8d 5e e4
    mov dx, strict word 0000ch                ; ba 0c 00
    mov di, word [bp-010h]                    ; 8b 7e f0
    call word [word di+00000h]                ; ff 95 00 00
    les bx, [bp-00ah]                         ; c4 5e f6
    mov word [es:bx+01ch], strict word 00000h ; 26 c7 47 1c 00 00
    test ax, ax                               ; 85 c0
    je short 03a9eh                           ; 74 06
    mov ax, strict word 0000dh                ; b8 0d 00
    jmp near 03b86h                           ; e9 e8 00
    mov es, [bp-006h]                         ; 8e 46 fa
    mov al, byte [es:si+001h]                 ; 26 8a 44 01
    cmp AL, strict byte 002h                  ; 3c 02
    jc short 03ab6h                           ; 72 0d
    jbe short 03aceh                          ; 76 23
    cmp AL, strict byte 004h                  ; 3c 04
    je short 03ae4h                           ; 74 35
    cmp AL, strict byte 003h                  ; 3c 03
    je short 03ad9h                           ; 74 26
    jmp near 03b2fh                           ; e9 79 00
    cmp AL, strict byte 001h                  ; 3c 01
    jne short 03b2fh                          ; 75 75
    mov es, [bp-006h]                         ; 8e 46 fa
    db  066h, 026h, 0c7h, 044h, 012h, 050h, 000h, 00fh, 000h
    ; mov dword [es:si+012h], strict dword 0000f0050h ; 66 26 c7 44 12 50 00 0f 00
    mov word [es:si+010h], strict word 00002h ; 26 c7 44 10 02 00
    jmp short 03b2fh                          ; eb 61
    db  066h, 026h, 0c7h, 044h, 012h, 050h, 000h, 012h, 000h
    ; mov dword [es:si+012h], strict dword 000120050h ; 66 26 c7 44 12 50 00 12 00
    jmp short 03ac6h                          ; eb ed
    db  066h, 026h, 0c7h, 044h, 012h, 050h, 000h, 024h, 000h
    ; mov dword [es:si+012h], strict dword 000240050h ; 66 26 c7 44 12 50 00 24 00
    jmp short 03ac6h                          ; eb e2
    mov dx, 001c4h                            ; ba c4 01
    mov ax, word [bp-00ch]                    ; 8b 46 f4
    call 01600h                               ; e8 13 db
    and AL, strict byte 03fh                  ; 24 3f
    xor ah, ah                                ; 30 e4
    mov es, [bp-006h]                         ; 8e 46 fa
    mov word [es:si+014h], ax                 ; 26 89 44 14
    mov dx, 001c4h                            ; ba c4 01
    mov ax, word [bp-00ch]                    ; 8b 46 f4
    call 01600h                               ; e8 ff da
    movzx bx, al                              ; 0f b6 d8
    sal bx, 002h                              ; c1 e3 02
    mov dx, 001c5h                            ; ba c5 01
    mov ax, word [bp-00ch]                    ; 8b 46 f4
    call 01600h                               ; e8 f0 da
    xor ah, ah                                ; 30 e4
    add ax, bx                                ; 01 d8
    inc ax                                    ; 40
    mov es, [bp-006h]                         ; 8e 46 fa
    mov word [es:si+012h], ax                 ; 26 89 44 12
    mov dx, 001c3h                            ; ba c3 01
    mov ax, word [bp-00ch]                    ; 8b 46 f4
    call 01600h                               ; e8 db da
    xor ah, ah                                ; 30 e4
    inc ax                                    ; 40
    mov es, [bp-006h]                         ; 8e 46 fa
    mov word [es:si+010h], ax                 ; 26 89 44 10
    mov es, [bp-006h]                         ; 8e 46 fa
    cmp byte [es:si+001h], 000h               ; 26 80 7c 01 00
    je short 03b6dh                           ; 74 34
    cmp byte [es:si+002h], 000h               ; 26 80 7c 02 00
    jne short 03b56h                          ; 75 16
    mov dx, strict word 00010h                ; ba 10 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 b7 da
    or AL, strict byte 041h                   ; 0c 41
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 00010h                ; ba 10 00
    mov ax, strict word 00040h                ; b8 40 00
    jmp short 03b6ah                          ; eb 14
    mov dx, 002c0h                            ; ba c0 02
    mov ax, word [bp-00eh]                    ; 8b 46 f2
    call 01600h                               ; e8 a1 da
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    movzx bx, al                              ; 0f b6 d8
    mov dx, 002c0h                            ; ba c0 02
    mov ax, word [bp-00eh]                    ; 8b 46 f2
    call 0160eh                               ; e8 a1 da
    mov es, [bp-006h]                         ; 8e 46 fa
    cmp byte [es:si+001h], 000h               ; 26 80 7c 01 00
    je short 03b7bh                           ; 74 04
    mov byte [es:si], 001h                    ; 26 c6 04 01
    mov es, [bp-006h]                         ; 8e 46 fa
    movzx ax, byte [es:si+002h]               ; 26 0f b6 44 02
    sal ax, 008h                              ; c1 e0 08
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
    push ax                                   ; 50
    dec si                                    ; 4e
    dec cx                                    ; 49
    dec ax                                    ; 48
    inc di                                    ; 47
    inc si                                    ; 46
    inc bp                                    ; 45
    inc sp                                    ; 44
    inc bx                                    ; 43
    inc dx                                    ; 42
    inc cx                                    ; 41
    sbb byte [01415h], dl                     ; 18 16 15 14
    adc word [bx+si], dx                      ; 11 10
    or ax, 00b0ch                             ; 0d 0c 0b
    or cl, byte [bx+di]                       ; 0a 09
    or byte [di], al                          ; 08 05
    add AL, strict byte 003h                  ; 04 03
    add al, byte [bx+di]                      ; 02 01
    add byte [bx+di+03fh], bh                 ; 00 79 3f
    mov bh, byte [si]                         ; 8a 3c
    sar word [si], 1                          ; d1 3c
    stc                                       ; f9
    cmp AL, strict byte 0c6h                  ; 3c c6
    cmp AL, strict byte 0f9h                  ; 3c f9
    cmp AL, strict byte 0c6h                  ; 3c c6
    cmp AL, strict byte 0cfh                  ; 3c cf
    db  03eh, 0afh
    ; ds scasw                                  ; 3e af
    cmp AL, strict byte 079h                  ; 3c 79
    aas                                       ; 3f
    jns short 03bffh                          ; 79 3f
    scasw                                     ; af
    cmp AL, strict byte 0afh                  ; 3c af
    cmp AL, strict byte 0afh                  ; 3c af
    cmp AL, strict byte 0afh                  ; 3c af
    cmp AL, strict byte 0afh                  ; 3c af
    cmp AL, strict byte 070h                  ; 3c 70
    aas                                       ; 3f
    scasw                                     ; af
    cmp AL, strict byte 079h                  ; 3c 79
    aas                                       ; 3f
    jns short 03c11h                          ; 79 3f
    jns short 03c13h                          ; 79 3f
    jns short 03c15h                          ; 79 3f
    jns short 03c17h                          ; 79 3f
    jns short 03c19h                          ; 79 3f
    jns short 03c1bh                          ; 79 3f
    jns short 03c1dh                          ; 79 3f
    jns short 03c1fh                          ; 79 3f
    jns short 03c21h                          ; 79 3f
    jns short 03c23h                          ; 79 3f
    jns short 03c25h                          ; 79 3f
_int13_cdemu:                                ; 0xf3be6 LB 0x42f
    push si                                   ; 56
    push di                                   ; 57
    enter 0002ah, 000h                        ; c8 2a 00 00
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 27 da
    mov di, 00322h                            ; bf 22 03
    mov cx, ax                                ; 89 c1
    mov si, di                                ; 89 fe
    mov word [bp-004h], ax                    ; 89 46 fc
    mov word [bp-00ah], 00122h                ; c7 46 f6 22 01
    mov word [bp-008h], ax                    ; 89 46 f8
    mov es, ax                                ; 8e c0
    mov al, byte [es:di+003h]                 ; 26 8a 45 03
    add al, al                                ; 00 c0
    mov byte [bp-002h], al                    ; 88 46 fe
    mov al, byte [es:di+004h]                 ; 26 8a 45 04
    add byte [bp-002h], al                    ; 00 46 fe
    xor bx, bx                                ; 31 db
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 ea d9
    mov es, cx                                ; 8e c1
    cmp byte [es:di], 000h                    ; 26 80 3d 00
    je short 03c3ah                           ; 74 0e
    movzx dx, byte [es:di+002h]               ; 26 0f b6 55 02
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    cmp dx, ax                                ; 39 c2
    je short 03c63h                           ; 74 29
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 69 dc
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 002bch                               ; 68 bc 02
    push 002c8h                               ; 68 c8 02
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 8c dc
    add sp, strict byte 0000ah                ; 83 c4 0a
    jmp near 03f99h                           ; e9 36 03
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    shr ax, 008h                              ; c1 e8 08
    cmp ax, strict word 00050h                ; 3d 50 00
    jnbe near 03f79h                          ; 0f 87 09 03
    push CS                                   ; 0e
    pop ES                                    ; 07
    mov cx, strict word 0001eh                ; b9 1e 00
    mov di, 03b8dh                            ; bf 8d 3b
    repne scasb                               ; f2 ae
    sal cx, 1                                 ; d1 e1
    mov di, cx                                ; 89 cf
    mov ax, word [cs:di+03baah]               ; 2e 8b 85 aa 3b
    mov bx, word [bp+01ah]                    ; 8b 5e 1a
    xor bh, bh                                ; 30 ff
    jmp ax                                    ; ff e0
    movzx bx, byte [bp-002h]                  ; 0f b6 5e fe
    imul bx, bx, strict byte 00018h           ; 6b db 18
    mov es, [bp-008h]                         ; 8e 46 f8
    add bx, word [bp-00ah]                    ; 03 5e f6
    movzx bx, byte [es:bx+01eh]               ; 26 0f b6 5f 1e
    add bx, bx                                ; 01 db
    cmp word [word bx+00000h], strict byte 00000h ; 83 bf 00 00 00
    je near 03cafh                            ; 0f 84 08 00
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    call word [word bx+0000ch]                ; ff 97 0c 00
    mov byte [bp+01bh], 000h                  ; c6 46 1b 00
    xor bx, bx                                ; 31 db
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 50 d9
    and byte [bp+020h], 0feh                  ; 80 66 20 fe
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    xor ah, ah                                ; 30 e4
    or ah, 003h                               ; 80 cc 03
    jmp near 03fa1h                           ; e9 d0 02
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 26 d9
    mov cl, al                                ; 88 c1
    movzx ax, cl                              ; 0f b6 c1
    sal ax, 008h                              ; c1 e0 08
    or bx, ax                                 ; 09 c3
    mov word [bp+01ah], bx                    ; 89 5e 1a
    xor bx, bx                                ; 31 db
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 1c d9
    test cl, cl                               ; 84 c9
    je short 03cb3h                           ; 74 bd
    jmp near 03fb5h                           ; e9 bc 02
    mov es, [bp-004h]                         ; 8e 46 fc
    mov di, word [es:si+014h]                 ; 26 8b 7c 14
    mov dx, word [es:si+012h]                 ; 26 8b 54 12
    mov bx, word [es:si+010h]                 ; 26 8b 5c 10
    mov ax, word [es:si+008h]                 ; 26 8b 44 08
    mov word [bp-010h], ax                    ; 89 46 f0
    mov ax, word [es:si+00ah]                 ; 26 8b 44 0a
    mov word [bp-00eh], ax                    ; 89 46 f2
    mov ax, word [bp+018h]                    ; 8b 46 18
    and ax, strict word 0003fh                ; 25 3f 00
    mov word [bp-00ch], ax                    ; 89 46 f4
    mov cx, word [bp+018h]                    ; 8b 4e 18
    and cx, 000c0h                            ; 81 e1 c0 00
    sal cx, 002h                              ; c1 e1 02
    mov ax, word [bp+018h]                    ; 8b 46 18
    shr ax, 008h                              ; c1 e8 08
    or ax, cx                                 ; 09 c8
    mov si, word [bp+016h]                    ; 8b 76 16
    shr si, 008h                              ; c1 ee 08
    mov cx, word [bp+01ah]                    ; 8b 4e 1a
    xor ch, ch                                ; 30 ed
    mov word [bp-006h], cx                    ; 89 4e fa
    test cx, cx                               ; 85 c9
    jne short 03d46h                          ; 75 03
    jmp near 03cafh                           ; e9 69 ff
    cmp di, word [bp-00ch]                    ; 3b 7e f4
    jc near 03f99h                            ; 0f 82 4c 02
    cmp ax, dx                                ; 39 d0
    jnc near 03f99h                           ; 0f 83 46 02
    cmp si, bx                                ; 39 de
    jnc near 03f99h                           ; 0f 83 40 02
    mov dx, word [bp+01ah]                    ; 8b 56 1a
    shr dx, 008h                              ; c1 ea 08
    cmp dx, strict byte 00004h                ; 83 fa 04
    jne short 03d67h                          ; 75 03
    jmp near 03cafh                           ; e9 48 ff
    mov dx, word [bp+014h]                    ; 8b 56 14
    shr dx, 004h                              ; c1 ea 04
    mov cx, word [bp+00ah]                    ; 8b 4e 0a
    add cx, dx                                ; 01 d1
    mov word [bp-012h], cx                    ; 89 4e ee
    mov dx, word [bp+014h]                    ; 8b 56 14
    and dx, strict byte 0000fh                ; 83 e2 0f
    mov word [bp-018h], dx                    ; 89 56 e8
    xor dl, dl                                ; 30 d2
    xor cx, cx                                ; 31 c9
    call 08fe9h                               ; e8 64 52
    xor bx, bx                                ; 31 db
    add ax, si                                ; 01 f0
    adc dx, bx                                ; 11 da
    mov bx, di                                ; 89 fb
    xor cx, cx                                ; 31 c9
    call 08fe9h                               ; e8 57 52
    mov bx, ax                                ; 89 c3
    mov ax, word [bp-00ch]                    ; 8b 46 f4
    dec ax                                    ; 48
    xor cx, cx                                ; 31 c9
    add ax, bx                                ; 01 d8
    adc dx, cx                                ; 11 ca
    mov bx, word [bp+01ah]                    ; 8b 5e 1a
    xor bl, bl                                ; 30 db
    mov cx, word [bp-006h]                    ; 8b 4e fa
    or cx, bx                                 ; 09 d9
    mov word [bp+01ah], cx                    ; 89 4e 1a
    mov si, ax                                ; 89 c6
    mov di, dx                                ; 89 d7
    shr di, 1                                 ; d1 ef
    rcr si, 1                                 ; d1 de
    shr di, 1                                 ; d1 ef
    rcr si, 1                                 ; d1 de
    mov word [bp-01ah], di                    ; 89 7e e6
    mov di, ax                                ; 89 c7
    and di, strict byte 00003h                ; 83 e7 03
    xor bh, bh                                ; 30 ff
    add ax, word [bp-006h]                    ; 03 46 fa
    adc dx, bx                                ; 11 da
    add ax, strict word 0ffffh                ; 05 ff ff
    adc dx, strict byte 0ffffh                ; 83 d2 ff
    mov word [bp-01eh], ax                    ; 89 46 e2
    mov word [bp-01ch], dx                    ; 89 56 e4
    shr word [bp-01ch], 1                     ; d1 6e e4
    rcr word [bp-01eh], 1                     ; d1 5e e2
    shr word [bp-01ch], 1                     ; d1 6e e4
    rcr word [bp-01eh], 1                     ; d1 5e e2
    mov cx, strict word 0000ch                ; b9 0c 00
    mov dx, ss                                ; 8c d2
    lea ax, [bp-02ah]                         ; 8d 46 d6
    call 0901ah                               ; e8 31 52
    mov word [bp-02ah], strict word 00028h    ; c7 46 d6 28 00
    mov ax, word [bp-010h]                    ; 8b 46 f0
    add ax, si                                ; 01 f0
    mov dx, word [bp-00eh]                    ; 8b 56 f2
    adc dx, word [bp-01ah]                    ; 13 56 e6
    xchg ah, al                               ; 86 c4
    xchg dh, dl                               ; 86 d6
    xchg dx, ax                               ; 92
    mov word [bp-028h], ax                    ; 89 46 d8
    mov word [bp-026h], dx                    ; 89 56 da
    mov ax, word [bp-01eh]                    ; 8b 46 e2
    sub ax, si                                ; 29 f0
    inc ax                                    ; 40
    xchg ah, al                               ; 86 c4
    mov word [bp-023h], ax                    ; 89 46 dd
    mov ax, word [bp-006h]                    ; 8b 46 fa
    les bx, [bp-00ah]                         ; c4 5e f6
    mov word [es:bx+00ah], ax                 ; 26 89 47 0a
    mov word [es:bx+00ch], 00200h             ; 26 c7 47 0c 00 02
    mov ax, di                                ; 89 f8
    sal ax, 009h                              ; c1 e0 09
    mov word [es:bx+01ah], ax                 ; 26 89 47 1a
    mov dx, word [bp-006h]                    ; 8b 56 fa
    xor dh, dh                                ; 30 f6
    and dl, 003h                              ; 80 e2 03
    mov bx, strict word 00004h                ; bb 04 00
    sub bx, dx                                ; 29 d3
    mov dx, bx                                ; 89 da
    sub dx, di                                ; 29 fa
    sal dx, 009h                              ; c1 e2 09
    and dh, 007h                              ; 80 e6 07
    mov bx, word [bp-00ah]                    ; 8b 5e f6
    mov word [es:bx+01ch], dx                 ; 26 89 57 1c
    movzx dx, byte [bp-002h]                  ; 0f b6 56 fe
    imul dx, dx, strict byte 00018h           ; 6b d2 18
    add bx, dx                                ; 01 d3
    movzx dx, byte [es:bx+01eh]               ; 26 0f b6 57 1e
    add dx, dx                                ; 01 d2
    mov word [bp-016h], dx                    ; 89 56 ea
    push word [bp-012h]                       ; ff 76 ee
    push word [bp-018h]                       ; ff 76 e8
    push strict byte 00001h                   ; 6a 01
    mov si, word [bp-006h]                    ; 8b 76 fa
    xor di, di                                ; 31 ff
    mov cx, strict word 00009h                ; b9 09 00
    sal si, 1                                 ; d1 e6
    rcl di, 1                                 ; d1 d7
    loop 03e69h                               ; e2 fa
    push di                                   ; 57
    push si                                   ; 56
    push ax                                   ; 50
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    mov cx, ss                                ; 8c d1
    lea bx, [bp-02ah]                         ; 8d 5e d6
    mov dx, strict word 0000ch                ; ba 0c 00
    mov si, word [bp-016h]                    ; 8b 76 ea
    call word [word si+00000h]                ; ff 94 00 00
    mov dx, ax                                ; 89 c2
    les bx, [bp-00ah]                         ; c4 5e f6
    db  066h, 026h, 0c7h, 047h, 01ah, 000h, 000h, 000h, 000h
    ; mov dword [es:bx+01ah], strict dword 000000000h ; 66 26 c7 47 1a 00 00 00 00
    test al, al                               ; 84 c0
    je near 03cafh                            ; 0f 84 16 fe
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 0a da
    movzx ax, dl                              ; 0f b6 c2
    push ax                                   ; 50
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 002bch                               ; 68 bc 02
    push 002feh                               ; 68 fe 02
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 2f da
    add sp, strict byte 0000ah                ; 83 c4 0a
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    xor ah, ah                                ; 30 e4
    or ah, 002h                               ; 80 cc 02
    mov word [bp+01ah], ax                    ; 89 46 1a
    mov byte [bp+01ah], 000h                  ; c6 46 1a 00
    jmp near 03fa4h                           ; e9 d5 00
    mov es, [bp-004h]                         ; 8e 46 fc
    mov di, word [es:si+014h]                 ; 26 8b 7c 14
    mov dx, word [es:si+012h]                 ; 26 8b 54 12
    dec dx                                    ; 4a
    mov bx, word [es:si+010h]                 ; 26 8b 5c 10
    dec bx                                    ; 4b
    mov byte [bp+01ah], 000h                  ; c6 46 1a 00
    mov ax, word [bp+014h]                    ; 8b 46 14
    xor al, al                                ; 30 c0
    mov cx, word [bp+018h]                    ; 8b 4e 18
    xor ch, ch                                ; 30 ed
    mov word [bp-016h], cx                    ; 89 4e ea
    mov cx, dx                                ; 89 d1
    xor ch, dh                                ; 30 f5
    sal cx, 008h                              ; c1 e1 08
    mov word [bp-014h], cx                    ; 89 4e ec
    mov cx, word [bp-016h]                    ; 8b 4e ea
    or cx, word [bp-014h]                     ; 0b 4e ec
    mov word [bp+018h], cx                    ; 89 4e 18
    shr dx, 002h                              ; c1 ea 02
    xor dh, dh                                ; 30 f6
    and dl, 0c0h                              ; 80 e2 c0
    and di, strict byte 0003fh                ; 83 e7 3f
    or dx, di                                 ; 09 fa
    xor cl, cl                                ; 30 c9
    or cx, dx                                 ; 09 d1
    mov word [bp+018h], cx                    ; 89 4e 18
    mov dx, word [bp+016h]                    ; 8b 56 16
    xor dh, dh                                ; 30 f6
    sal bx, 008h                              ; c1 e3 08
    or dx, bx                                 ; 09 da
    mov word [bp+016h], dx                    ; 89 56 16
    xor dl, dl                                ; 30 d2
    or dl, 002h                               ; 80 ca 02
    mov word [bp+016h], dx                    ; 89 56 16
    mov dl, byte [es:si+001h]                 ; 26 8a 54 01
    mov word [bp+014h], ax                    ; 89 46 14
    cmp dl, 003h                              ; 80 fa 03
    je short 03f53h                           ; 74 1a
    cmp dl, 002h                              ; 80 fa 02
    je short 03f4fh                           ; 74 11
    cmp dl, 001h                              ; 80 fa 01
    jne short 03f57h                          ; 75 14
    mov ax, word [bp+014h]                    ; 8b 46 14
    xor al, al                                ; 30 c0
    or AL, strict byte 002h                   ; 0c 02
    mov word [bp+014h], ax                    ; 89 46 14
    jmp short 03f57h                          ; eb 08
    or AL, strict byte 004h                   ; 0c 04
    jmp short 03f4ah                          ; eb f7
    or AL, strict byte 005h                   ; 0c 05
    jmp short 03f4ah                          ; eb f3
    mov es, [bp-004h]                         ; 8e 46 fc
    cmp byte [es:si+001h], 004h               ; 26 80 7c 01 04
    jnc near 03cafh                           ; 0f 83 4c fd
    mov word [bp+00ch], 0efc7h                ; c7 46 0c c7 ef
    mov word [bp+00ah], 0f000h                ; c7 46 0a 00 f0
    jmp near 03cafh                           ; e9 3f fd
    or bh, 003h                               ; 80 cf 03
    mov word [bp+01ah], bx                    ; 89 5e 1a
    jmp near 03cb3h                           ; e9 3a fd
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 2a d9
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 002bch                               ; 68 bc 02
    push 0031fh                               ; 68 1f 03
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 53 d9
    add sp, strict byte 00008h                ; 83 c4 08
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    xor ah, ah                                ; 30 e4
    or ah, 001h                               ; 80 cc 01
    mov word [bp+01ah], ax                    ; 89 46 1a
    mov bx, word [bp+01ah]                    ; 8b 5e 1a
    shr bx, 008h                              ; c1 eb 08
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 59 d6
    or byte [bp+020h], 001h                   ; 80 4e 20 01
    jmp near 03cc2h                           ; e9 06 fd
    push ax                                   ; 50
    dec si                                    ; 4e
    dec cx                                    ; 49
    dec ax                                    ; 48
    inc di                                    ; 47
    inc si                                    ; 46
    inc bp                                    ; 45
    inc sp                                    ; 44
    inc bx                                    ; 43
    inc dx                                    ; 42
    inc cx                                    ; 41
    sbb byte [01415h], dl                     ; 18 16 15 14
    adc word [bx+si], dx                      ; 11 10
    or ax, 00b0ch                             ; 0d 0c 0b
    or cl, byte [bx+di]                       ; 0a 09
    or byte [di], al                          ; 08 05
    add AL, strict byte 003h                  ; 04 03
    add al, byte [bx+di]                      ; 02 01
    add byte [di+045h], dl                    ; 00 55 45
    aam 042h                                  ; d4 42
    rol byte [bx+si+055h], 045h               ; c0 40 55 45
    mov CH, strict byte 040h                  ; b5 40
    push bp                                   ; 55
    inc bp                                    ; 45
    mov CH, strict byte 040h                  ; b5 40
    push bp                                   ; 55
    inc bp                                    ; 45
    aam 042h                                  ; d4 42
    push bp                                   ; 55
    inc bp                                    ; 45
    push bp                                   ; 55
    inc bp                                    ; 45
    aam 042h                                  ; d4 42
    aam 042h                                  ; d4 42
    aam 042h                                  ; d4 42
    aam 042h                                  ; d4 42
    aam 042h                                  ; d4 42
    jmp far 05542h:0d440h                     ; ea 40 d4 42 55
    inc bp                                    ; 45
    rep inc ax                                ; f3 40
    push ES                                   ; 06
    inc cx                                    ; 41
    mov CH, strict byte 040h                  ; b5 40
    push ES                                   ; 06
    inc cx                                    ; 41
    xor AL, strict byte 042h                  ; 34 42
    jmp short 0404dh                          ; eb 42
    push ES                                   ; 06
    inc cx                                    ; 41
    adc al, byte [bp+di+00eh]                 ; 12 43 0e
    inc bp                                    ; 45
    push SS                                   ; 16
    inc bp                                    ; 45
    push bp                                   ; 55
    inc bp                                    ; 45
_int13_cdrom:                                ; 0xf4015 LB 0x55d
    push si                                   ; 56
    push di                                   ; 57
    enter 00028h, 000h                        ; c8 28 00 00
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 f8 d5
    mov word [bp-016h], ax                    ; 89 46 ea
    mov si, 00122h                            ; be 22 01
    mov word [bp-008h], ax                    ; 89 46 f8
    xor bx, bx                                ; 31 db
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 d6 d5
    mov ax, word [bp+014h]                    ; 8b 46 14
    xor ah, ah                                ; 30 e4
    cmp ax, 000e0h                            ; 3d e0 00
    jc short 04047h                           ; 72 05
    cmp ax, 000f0h                            ; 3d f0 00
    jc short 04065h                           ; 72 1e
    mov ax, word [bp+014h]                    ; 8b 46 14
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, word [bp+01ch]                    ; 8b 46 1c
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 0034fh                               ; 68 4f 03
    push 0035bh                               ; 68 5b 03
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 8a d8
    add sp, strict byte 0000ah                ; 83 c4 0a
    jmp near 04532h                           ; e9 cd 04
    mov ax, word [bp+014h]                    ; 8b 46 14
    xor ah, ah                                ; 30 e4
    mov es, [bp-008h]                         ; 8e 46 f8
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    mov dl, byte [es:bx+000d0h]               ; 26 8a 97 d0 00
    mov byte [bp-004h], dl                    ; 88 56 fc
    cmp dl, 010h                              ; 80 fa 10
    jc short 0408eh                           ; 72 10
    push ax                                   ; 50
    mov ax, word [bp+01ch]                    ; 8b 46 1c
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 0034fh                               ; 68 4f 03
    push 00386h                               ; 68 86 03
    jmp short 0405ah                          ; eb cc
    mov ax, word [bp+01ch]                    ; 8b 46 1c
    shr ax, 008h                              ; c1 e8 08
    cmp ax, strict word 00050h                ; 3d 50 00
    jnbe near 04555h                          ; 0f 87 ba 04
    push CS                                   ; 0e
    pop ES                                    ; 07
    mov cx, strict word 0001eh                ; b9 1e 00
    mov di, 03fbch                            ; bf bc 3f
    repne scasb                               ; f2 ae
    sal cx, 1                                 ; d1 e1
    mov di, cx                                ; 89 cf
    mov ax, word [cs:di+03fd9h]               ; 2e 8b 85 d9 3f
    mov bx, word [bp+01ch]                    ; 8b 5e 1c
    xor bh, bh                                ; 30 ff
    jmp ax                                    ; ff e0
    mov ax, word [bp+01ch]                    ; 8b 46 1c
    xor ah, ah                                ; 30 e4
    or ah, 003h                               ; 80 cc 03
    jmp near 0453ah                           ; e9 7a 04
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 37 d5
    mov cl, al                                ; 88 c1
    movzx ax, cl                              ; 0f b6 c1
    sal ax, 008h                              ; c1 e0 08
    or bx, ax                                 ; 09 c3
    mov word [bp+01ch], bx                    ; 89 5e 1c
    xor bx, bx                                ; 31 db
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 2d d5
    test cl, cl                               ; 84 c9
    je near 042d8h                            ; 0f 84 f1 01
    jmp near 0454eh                           ; e9 64 04
    or bh, 002h                               ; 80 cf 02
    mov word [bp+01ch], bx                    ; 89 5e 1c
    jmp near 0453dh                           ; e9 4a 04
    mov word [bp+016h], 0aa55h                ; c7 46 16 55 aa
    or bh, 030h                               ; 80 cf 30
    mov word [bp+01ch], bx                    ; 89 5e 1c
    mov word [bp+01ah], strict word 00007h    ; c7 46 1a 07 00
    jmp near 042d8h                           ; e9 d2 01
    mov bx, word [bp+010h]                    ; 8b 5e 10
    mov es, [bp+00ah]                         ; 8e 46 0a
    mov word [bp-010h], bx                    ; 89 5e f0
    mov [bp-00eh], es                         ; 8c 46 f2
    mov ax, word [es:bx+002h]                 ; 26 8b 47 02
    mov word [bp-00ch], ax                    ; 89 46 f4
    mov ax, word [es:bx+006h]                 ; 26 8b 47 06
    mov word [bp-018h], ax                    ; 89 46 e8
    mov ax, word [es:bx+004h]                 ; 26 8b 47 04
    mov word [bp-01ch], ax                    ; 89 46 e4
    mov ax, word [es:bx+00ch]                 ; 26 8b 47 0c
    mov word [bp-014h], ax                    ; 89 46 ec
    mov di, word [es:bx+00eh]                 ; 26 8b 7f 0e
    or di, ax                                 ; 09 c7
    je short 0414eh                           ; 74 18
    mov ax, word [bp+01ch]                    ; 8b 46 1c
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 0034fh                               ; 68 4f 03
    push 003b8h                               ; 68 b8 03
    push strict byte 00007h                   ; 6a 07
    call 018e9h                               ; e8 a1 d7
    add sp, strict byte 00008h                ; 83 c4 08
    jmp near 04532h                           ; e9 e4 03
    les bx, [bp-010h]                         ; c4 5e f0
    mov ax, word [es:bx+008h]                 ; 26 8b 47 08
    mov word [bp-014h], ax                    ; 89 46 ec
    mov di, bx                                ; 89 df
    mov di, word [es:di+00ah]                 ; 26 8b 7d 0a
    mov ax, word [bp+01ch]                    ; 8b 46 1c
    shr ax, 008h                              ; c1 e8 08
    mov word [bp-012h], ax                    ; 89 46 ee
    cmp ax, strict word 00044h                ; 3d 44 00
    je near 042d4h                            ; 0f 84 66 01
    cmp ax, strict word 00047h                ; 3d 47 00
    je near 042d4h                            ; 0f 84 5f 01
    mov cx, strict word 0000ch                ; b9 0c 00
    xor bx, bx                                ; 31 db
    mov dx, ss                                ; 8c d2
    lea ax, [bp-028h]                         ; 8d 46 d8
    call 0901ah                               ; e8 98 4e
    mov word [bp-028h], strict word 00028h    ; c7 46 d8 28 00
    mov ax, word [bp-014h]                    ; 8b 46 ec
    mov dx, di                                ; 89 fa
    xchg ah, al                               ; 86 c4
    xchg dh, dl                               ; 86 d6
    xchg dx, ax                               ; 92
    mov word [bp-026h], ax                    ; 89 46 da
    mov word [bp-024h], dx                    ; 89 56 dc
    mov ax, word [bp-00ch]                    ; 8b 46 f4
    xchg ah, al                               ; 86 c4
    mov word [bp-021h], ax                    ; 89 46 df
    mov ax, word [bp-00ch]                    ; 8b 46 f4
    mov es, [bp-008h]                         ; 8e 46 f8
    mov word [es:si+00ah], ax                 ; 26 89 44 0a
    mov word [es:si+00ch], 00800h             ; 26 c7 44 0c 00 08
    movzx bx, byte [bp-004h]                  ; 0f b6 5e fc
    imul bx, bx, strict byte 00018h           ; 6b db 18
    add bx, si                                ; 01 f3
    movzx di, byte [es:bx+01eh]               ; 26 0f b6 7f 1e
    add di, di                                ; 01 ff
    push word [bp-018h]                       ; ff 76 e8
    push word [bp-01ch]                       ; ff 76 e4
    push strict byte 00001h                   ; 6a 01
    xor bx, bx                                ; 31 db
    mov cx, strict word 0000bh                ; b9 0b 00
    sal ax, 1                                 ; d1 e0
    rcl bx, 1                                 ; d1 d3
    loop 041cch                               ; e2 fa
    push bx                                   ; 53
    push ax                                   ; 50
    push strict byte 00000h                   ; 6a 00
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc
    mov cx, ss                                ; 8c d1
    lea bx, [bp-028h]                         ; 8d 5e d8
    mov dx, strict word 0000ch                ; ba 0c 00
    call word [word di+00000h]                ; ff 95 00 00
    mov dx, ax                                ; 89 c2
    mov es, [bp-008h]                         ; 8e 46 f8
    mov ax, word [es:si+016h]                 ; 26 8b 44 16
    mov di, word [es:si+018h]                 ; 26 8b 7c 18
    mov cx, strict word 0000bh                ; b9 0b 00
    shr di, 1                                 ; d1 ef
    rcr ax, 1                                 ; d1 d8
    loop 041f6h                               ; e2 fa
    les bx, [bp-010h]                         ; c4 5e f0
    mov word [es:bx+002h], ax                 ; 26 89 47 02
    test dl, dl                               ; 84 d2
    je near 042d4h                            ; 0f 84 cb 00
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 9a d6
    movzx ax, dl                              ; 0f b6 c2
    push ax                                   ; 50
    push word [bp-012h]                       ; ff 76 ee
    push 0034fh                               ; 68 4f 03
    push 003e1h                               ; 68 e1 03
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 c3 d6
    add sp, strict byte 0000ah                ; 83 c4 0a
    mov ax, word [bp+01ch]                    ; 8b 46 1c
    xor ah, ah                                ; 30 e4
    or ah, 00ch                               ; 80 cc 0c
    jmp near 0453ah                           ; e9 06 03
    cmp bx, strict byte 00002h                ; 83 fb 02
    jnbe near 04532h                          ; 0f 87 f7 02
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    mov es, [bp-008h]                         ; 8e 46 f8
    mov di, si                                ; 89 f7
    add di, ax                                ; 01 c7
    mov al, byte [es:di+021h]                 ; 26 8a 45 21
    cmp bx, strict byte 00002h                ; 83 fb 02
    je short 042c5h                           ; 74 73
    cmp bx, strict byte 00001h                ; 83 fb 01
    je short 04292h                           ; 74 3b
    test bx, bx                               ; 85 db
    jne near 042d4h                           ; 0f 85 77 00
    cmp AL, strict byte 0ffh                  ; 3c ff
    jne short 04273h                          ; 75 12
    mov ax, word [bp+01ch]                    ; 8b 46 1c
    xor ah, ah                                ; 30 e4
    or ah, 0b4h                               ; 80 cc b4
    mov word [bp+01ch], ax                    ; 89 46 1c
    xor al, al                                ; 30 c0
    or AL, strict byte 001h                   ; 0c 01
    jmp near 0453ah                           ; e9 c7 02
    movzx dx, byte [bp-004h]                  ; 0f b6 56 fc
    imul dx, dx, strict byte 00018h           ; 6b d2 18
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    mov es, [bp-008h]                         ; 8e 46 f8
    add si, dx                                ; 01 d6
    mov byte [es:si+021h], al                 ; 26 88 44 21
    mov ax, word [bp+01ch]                    ; 8b 46 1c
    xor al, al                                ; 30 c0
    or AL, strict byte 001h                   ; 0c 01
    mov word [bp+01ch], ax                    ; 89 46 1c
    jmp near 042d4h                           ; e9 42 00
    test al, al                               ; 84 c0
    jne short 042a2h                          ; 75 0c
    or bh, 0b0h                               ; 80 cf b0
    mov word [bp+01ch], bx                    ; 89 5e 1c
    mov byte [bp+01ch], al                    ; 88 46 1c
    jmp near 0453dh                           ; e9 9b 02
    movzx dx, byte [bp-004h]                  ; 0f b6 56 fc
    imul dx, dx, strict byte 00018h           ; 6b d2 18
    db  0feh, 0c8h
    ; dec al                                    ; fe c8
    mov es, [bp-008h]                         ; 8e 46 f8
    add si, dx                                ; 01 d6
    mov byte [es:si+021h], al                 ; 26 88 44 21
    test al, al                               ; 84 c0
    db  00fh, 095h, 0c0h
    ; setne al                                  ; 0f 95 c0
    movzx dx, al                              ; 0f b6 d0
    mov ax, word [bp+01ch]                    ; 8b 46 1c
    xor al, al                                ; 30 c0
    or ax, dx                                 ; 09 d0
    jmp short 0428ch                          ; eb c7
    test al, al                               ; 84 c0
    db  00fh, 095h, 0c0h
    ; setne al                                  ; 0f 95 c0
    xor ah, ah                                ; 30 e4
    mov dx, word [bp+01ch]                    ; 8b 56 1c
    mov dl, al                                ; 88 c2
    mov word [bp+01ch], dx                    ; 89 56 1c
    mov byte [bp+01dh], 000h                  ; c6 46 1d 00
    xor bx, bx                                ; 31 db
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 2b d3
    and byte [bp+022h], 0feh                  ; 80 66 22 fe
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    mov es, [bp-008h]                         ; 8e 46 f8
    add si, ax                                ; 01 c6
    mov al, byte [es:si+021h]                 ; 26 8a 44 21
    test al, al                               ; 84 c0
    je short 04305h                           ; 74 06
    or bh, 0b1h                               ; 80 cf b1
    jmp near 040edh                           ; e9 e8 fd
    je short 042d4h                           ; 74 cd
    mov ax, word [bp+01ch]                    ; 8b 46 1c
    xor ah, ah                                ; 30 e4
    or ah, 0b1h                               ; 80 cc b1
    jmp near 0453ah                           ; e9 28 02
    mov dx, word [bp+010h]                    ; 8b 56 10
    mov cx, word [bp+00ah]                    ; 8b 4e 0a
    mov bx, dx                                ; 89 d3
    mov word [bp-006h], cx                    ; 89 4e fa
    mov es, cx                                ; 8e c1
    mov di, dx                                ; 89 d7
    mov ax, word [es:di]                      ; 26 8b 05
    mov word [bp-00ah], ax                    ; 89 46 f6
    cmp ax, strict word 0001ah                ; 3d 1a 00
    jc near 04532h                            ; 0f 82 04 02
    jc short 0437fh                           ; 72 4f
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    mov es, [bp-008h]                         ; 8e 46 f8
    mov di, si                                ; 89 f7
    add di, ax                                ; 01 c7
    mov ax, word [es:di+024h]                 ; 26 8b 45 24
    mov es, cx                                ; 8e c1
    mov di, dx                                ; 89 d7
    db  066h, 026h, 0c7h, 005h, 01ah, 000h, 074h, 000h
    ; mov dword [es:di], strict dword 00074001ah ; 66 26 c7 05 1a 00 74 00
    db  066h, 026h, 0c7h, 045h, 004h, 0ffh, 0ffh, 0ffh, 0ffh
    ; mov dword [es:di+004h], strict dword 0ffffffffh ; 66 26 c7 45 04 ff ff ff ff
    db  066h, 026h, 0c7h, 045h, 008h, 0ffh, 0ffh, 0ffh, 0ffh
    ; mov dword [es:di+008h], strict dword 0ffffffffh ; 66 26 c7 45 08 ff ff ff ff
    db  066h, 026h, 0c7h, 045h, 00ch, 0ffh, 0ffh, 0ffh, 0ffh
    ; mov dword [es:di+00ch], strict dword 0ffffffffh ; 66 26 c7 45 0c ff ff ff ff
    mov word [es:di+018h], ax                 ; 26 89 45 18
    db  066h, 026h, 0c7h, 045h, 010h, 0ffh, 0ffh, 0ffh, 0ffh
    ; mov dword [es:di+010h], strict dword 0ffffffffh ; 66 26 c7 45 10 ff ff ff ff
    db  066h, 026h, 0c7h, 045h, 014h, 0ffh, 0ffh, 0ffh, 0ffh
    ; mov dword [es:di+014h], strict dword 0ffffffffh ; 66 26 c7 45 14 ff ff ff ff
    cmp word [bp-00ah], strict byte 0001eh    ; 83 7e f6 1e
    jc near 04456h                            ; 0f 82 cf 00
    mov es, [bp-006h]                         ; 8e 46 fa
    mov word [es:bx], strict word 0001eh      ; 26 c7 07 1e 00
    mov ax, word [bp-016h]                    ; 8b 46 ea
    mov word [es:bx+01ch], ax                 ; 26 89 47 1c
    mov word [es:bx+01ah], 00312h             ; 26 c7 47 1a 12 03
    movzx cx, byte [bp-004h]                  ; 0f b6 4e fc
    mov ax, cx                                ; 89 c8
    cwd                                       ; 99
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2
    sar ax, 1                                 ; d1 f8
    movzx di, al                              ; 0f b6 f8
    imul di, di, strict byte 00006h           ; 6b ff 06
    mov es, [bp-008h]                         ; 8e 46 f8
    add di, si                                ; 01 f7
    mov ax, word [es:di+001c2h]               ; 26 8b 85 c2 01
    mov word [bp-01ah], ax                    ; 89 46 e6
    mov dx, word [es:di+001c4h]               ; 26 8b 95 c4 01
    mov al, byte [es:di+001c1h]               ; 26 8a 85 c1 01
    mov byte [bp-002h], al                    ; 88 46 fe
    imul cx, cx, strict byte 00018h           ; 6b c9 18
    mov di, si                                ; 89 f7
    add di, cx                                ; 01 cf
    mov al, byte [es:di+022h]                 ; 26 8a 45 22
    cmp AL, strict byte 001h                  ; 3c 01
    db  00fh, 094h, 0c0h
    ; sete al                                   ; 0f 94 c0
    xor ah, ah                                ; 30 e4
    or AL, strict byte 070h                   ; 0c 70
    mov di, ax                                ; 89 c7
    mov ax, word [bp-01ah]                    ; 8b 46 e6
    mov word [es:si+001f0h], ax               ; 26 89 84 f0 01
    mov word [es:si+001f2h], dx               ; 26 89 94 f2 01
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc
    cwd                                       ; 99
    mov cx, strict word 00002h                ; b9 02 00
    idiv cx                                   ; f7 f9
    or dl, 00eh                               ; 80 ca 0e
    mov ax, dx                                ; 89 d0
    sal ax, 004h                              ; c1 e0 04
    mov byte [es:si+001f4h], al               ; 26 88 84 f4 01
    mov byte [es:si+001f5h], 0cbh             ; 26 c6 84 f5 01 cb
    mov al, byte [bp-002h]                    ; 8a 46 fe
    mov byte [es:si+001f6h], al               ; 26 88 84 f6 01
    mov word [es:si+001f7h], strict word 00001h ; 26 c7 84 f7 01 01 00
    mov byte [es:si+001f9h], 000h             ; 26 c6 84 f9 01 00
    mov word [es:si+001fah], di               ; 26 89 bc fa 01
    mov word [es:si+001fch], strict word 00000h ; 26 c7 84 fc 01 00 00
    mov byte [es:si+001feh], 011h             ; 26 c6 84 fe 01 11
    xor cl, cl                                ; 30 c9
    xor ch, ch                                ; 30 ed
    jmp short 04439h                          ; eb 05
    cmp ch, 00fh                              ; 80 fd 0f
    jnc short 0444ch                          ; 73 13
    movzx dx, ch                              ; 0f b6 d5
    add dx, 00312h                            ; 81 c2 12 03
    mov ax, word [bp-016h]                    ; 8b 46 ea
    call 01600h                               ; e8 ba d1
    add cl, al                                ; 00 c1
    db  0feh, 0c5h
    ; inc ch                                    ; fe c5
    jmp short 04434h                          ; eb e8
    neg cl                                    ; f6 d9
    mov es, [bp-008h]                         ; 8e 46 f8
    mov byte [es:si+001ffh], cl               ; 26 88 8c ff 01
    cmp word [bp-00ah], strict byte 00042h    ; 83 7e f6 42
    jc near 042d4h                            ; 0f 82 76 fe
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc
    cwd                                       ; 99
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2
    sar ax, 1                                 ; d1 f8
    xor ah, ah                                ; 30 e4
    imul ax, ax, strict byte 00006h           ; 6b c0 06
    mov es, [bp-008h]                         ; 8e 46 f8
    add si, ax                                ; 01 c6
    mov al, byte [es:si+001c0h]               ; 26 8a 84 c0 01
    mov dx, word [es:si+001c2h]               ; 26 8b 94 c2 01
    mov es, [bp-006h]                         ; 8e 46 fa
    mov word [es:bx], strict word 00042h      ; 26 c7 07 42 00
    db  066h, 026h, 0c7h, 047h, 01eh, 0ddh, 0beh, 024h, 000h
    ; mov dword [es:bx+01eh], strict dword 00024beddh ; 66 26 c7 47 1e dd be 24 00
    mov word [es:bx+022h], strict word 00000h ; 26 c7 47 22 00 00
    test al, al                               ; 84 c0
    jne short 0449fh                          ; 75 09
    db  066h, 026h, 0c7h, 047h, 024h, 049h, 053h, 041h, 020h
    ; mov dword [es:bx+024h], strict dword 020415349h ; 66 26 c7 47 24 49 53 41 20
    mov es, [bp-006h]                         ; 8e 46 fa
    db  066h, 026h, 0c7h, 047h, 028h, 041h, 054h, 041h, 020h
    ; mov dword [es:bx+028h], strict dword 020415441h ; 66 26 c7 47 28 41 54 41 20
    db  066h, 026h, 0c7h, 047h, 02ch, 020h, 020h, 020h, 020h
    ; mov dword [es:bx+02ch], strict dword 020202020h ; 66 26 c7 47 2c 20 20 20 20
    test al, al                               ; 84 c0
    jne short 044cbh                          ; 75 13
    mov word [es:bx+030h], dx                 ; 26 89 57 30
    db  066h, 026h, 0c7h, 047h, 032h, 000h, 000h, 000h, 000h
    ; mov dword [es:bx+032h], strict dword 000000000h ; 66 26 c7 47 32 00 00 00 00
    mov word [es:bx+036h], strict word 00000h ; 26 c7 47 36 00 00
    mov al, byte [bp-004h]                    ; 8a 46 fc
    and AL, strict byte 001h                  ; 24 01
    xor ah, ah                                ; 30 e4
    mov es, [bp-006h]                         ; 8e 46 fa
    mov word [es:bx+038h], ax                 ; 26 89 47 38
    db  066h, 026h, 0c7h, 047h, 03ah, 000h, 000h, 000h, 000h
    ; mov dword [es:bx+03ah], strict dword 000000000h ; 66 26 c7 47 3a 00 00 00 00
    mov word [es:bx+03eh], strict word 00000h ; 26 c7 47 3e 00 00
    xor al, al                                ; 30 c0
    mov AH, strict byte 01eh                  ; b4 1e
    jmp short 044f3h                          ; eb 05
    cmp ah, 040h                              ; 80 fc 40
    jnc short 04502h                          ; 73 0f
    movzx si, ah                              ; 0f b6 f4
    mov es, [bp-006h]                         ; 8e 46 fa
    add si, bx                                ; 01 de
    add al, byte [es:si]                      ; 26 02 04
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4
    jmp short 044eeh                          ; eb ec
    neg al                                    ; f6 d8
    mov es, [bp-006h]                         ; 8e 46 fa
    mov byte [es:bx+041h], al                 ; 26 88 47 41
    jmp near 042d4h                           ; e9 c6 fd
    or bh, 006h                               ; 80 cf 06
    mov word [bp+01ch], bx                    ; 89 5e 1c
    jmp short 0454eh                          ; eb 38
    cmp bx, strict byte 00006h                ; 83 fb 06
    je near 042d4h                            ; 0f 84 b7 fd
    cmp bx, strict byte 00001h                ; 83 fb 01
    jc short 04532h                           ; 72 10
    jbe near 042d4h                           ; 0f 86 ae fd
    cmp bx, strict byte 00003h                ; 83 fb 03
    jc short 04532h                           ; 72 07
    cmp bx, strict byte 00004h                ; 83 fb 04
    jbe near 042d4h                           ; 0f 86 a2 fd
    mov ax, word [bp+01ch]                    ; 8b 46 1c
    xor ah, ah                                ; 30 e4
    or ah, 001h                               ; 80 cc 01
    mov word [bp+01ch], ax                    ; 89 46 1c
    mov bx, word [bp+01ch]                    ; 8b 5e 1c
    shr bx, 008h                              ; c1 eb 08
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 c0 d0
    or byte [bp+022h], 001h                   ; 80 4e 22 01
    jmp near 042e7h                           ; e9 92 fd
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 4e d3
    mov ax, word [bp+01ch]                    ; 8b 46 1c
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 0034fh                               ; 68 4f 03
    push 002a3h                               ; 68 a3 02
    push strict byte 00004h                   ; 6a 04
    jmp near 04145h                           ; e9 d3 fb
print_boot_device_:                          ; 0xf4572 LB 0x48
    push cx                                   ; 51
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    test al, al                               ; 84 c0
    je short 0457fh                           ; 74 05
    mov dx, strict word 00002h                ; ba 02 00
    jmp short 04599h                          ; eb 1a
    test dl, dl                               ; 84 d2
    je short 04588h                           ; 74 05
    mov dx, strict word 00003h                ; ba 03 00
    jmp short 04599h                          ; eb 11
    test bl, 080h                             ; f6 c3 80
    jne short 04591h                          ; 75 04
    xor dh, dh                                ; 30 f6
    jmp short 04599h                          ; eb 08
    test bl, 080h                             ; f6 c3 80
    je short 045b7h                           ; 74 21
    mov dx, strict word 00001h                ; ba 01 00
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 0a d3
    imul dx, dx, strict byte 0000ah           ; 6b d2 0a
    add dx, 00d58h                            ; 81 c2 58 0d
    push dx                                   ; 52
    push 00404h                               ; 68 04 04
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 35 d3
    add sp, strict byte 00006h                ; 83 c4 06
    pop bp                                    ; 5d
    pop cx                                    ; 59
    retn                                      ; c3
print_boot_failure_:                         ; 0xf45ba LB 0x90
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dh, cl                                ; 88 ce
    mov ah, bl                                ; 88 dc
    and ah, 07fh                              ; 80 e4 7f
    movzx si, ah                              ; 0f b6 f4
    test al, al                               ; 84 c0
    je short 045e7h                           ; 74 1b
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 d7 d2
    push 00d6ch                               ; 68 6c 0d
    push 00418h                               ; 68 18 04
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 07 d3
    add sp, strict byte 00006h                ; 83 c4 06
    jmp short 0462bh                          ; eb 44
    test dl, dl                               ; 84 d2
    je short 045fbh                           ; 74 10
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 b8 d2
    push 00d76h                               ; 68 76 0d
    jmp short 045dah                          ; eb df
    test bl, 080h                             ; f6 c3 80
    je short 04611h                           ; 74 11
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 a3 d2
    push si                                   ; 56
    push 00d62h                               ; 68 62 0d
    jmp short 04620h                          ; eb 0f
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 92 d2
    push si                                   ; 56
    push 00d58h                               ; 68 58 0d
    push 0042dh                               ; 68 2d 04
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 c1 d2
    add sp, strict byte 00008h                ; 83 c4 08
    cmp byte [bp+006h], 001h                  ; 80 7e 06 01
    jne short 04645h                          ; 75 14
    test dh, dh                               ; 84 f6
    jne short 0463ah                          ; 75 05
    push 00445h                               ; 68 45 04
    jmp short 0463dh                          ; eb 03
    push 0046fh                               ; 68 6f 04
    push strict byte 00007h                   ; 6a 07
    call 018e9h                               ; e8 a7 d2
    add sp, strict byte 00004h                ; 83 c4 04
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn 00002h                               ; c2 02 00
print_cdromboot_failure_:                    ; 0xf464a LB 0x24
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, ax                                ; 89 c2
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 51 d2
    push dx                                   ; 52
    push 004a4h                               ; 68 a4 04
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 83 d2
    add sp, strict byte 00006h                ; 83 c4 06
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
_int19_function:                             ; 0xf466e LB 0x251
    push si                                   ; 56
    push di                                   ; 57
    enter 0000eh, 000h                        ; c8 0e 00 00
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 9f cf
    mov bx, ax                                ; 89 c3
    mov word [bp-00ah], ax                    ; 89 46 f6
    mov byte [bp-004h], 000h                  ; c6 46 fc 00
    mov ax, strict word 0003dh                ; b8 3d 00
    call 0165ch                               ; e8 d0 cf
    movzx si, al                              ; 0f b6 f0
    mov ax, strict word 00038h                ; b8 38 00
    call 0165ch                               ; e8 c7 cf
    and AL, strict byte 0f0h                  ; 24 f0
    xor ah, ah                                ; 30 e4
    sal ax, 004h                              ; c1 e0 04
    or si, ax                                 ; 09 c6
    mov ax, strict word 0003ch                ; b8 3c 00
    call 0165ch                               ; e8 b8 cf
    and AL, strict byte 00fh                  ; 24 0f
    xor ah, ah                                ; 30 e4
    sal ax, 00ch                              ; c1 e0 0c
    or si, ax                                 ; 09 c6
    mov dx, 00339h                            ; ba 39 03
    mov ax, bx                                ; 89 d8
    call 01600h                               ; e8 4b cf
    test al, al                               ; 84 c0
    je short 046c4h                           ; 74 0b
    mov dx, 00339h                            ; ba 39 03
    mov ax, bx                                ; 89 d8
    call 01600h                               ; e8 3f cf
    movzx si, al                              ; 0f b6 f0
    cmp byte [bp+008h], 001h                  ; 80 7e 08 01
    jne short 046dah                          ; 75 10
    mov ax, strict word 0003ch                ; b8 3c 00
    call 0165ch                               ; e8 8c cf
    and AL, strict byte 0f0h                  ; 24 f0
    xor ah, ah                                ; 30 e4
    sar ax, 004h                              ; c1 f8 04
    call 072b6h                               ; e8 dc 2b
    cmp byte [bp+008h], 002h                  ; 80 7e 08 02
    jne short 046e3h                          ; 75 03
    shr si, 004h                              ; c1 ee 04
    cmp byte [bp+008h], 003h                  ; 80 7e 08 03
    jne short 046ech                          ; 75 03
    shr si, 008h                              ; c1 ee 08
    cmp byte [bp+008h], 004h                  ; 80 7e 08 04
    jne short 046f5h                          ; 75 03
    shr si, 00ch                              ; c1 ee 0c
    cmp si, strict byte 00010h                ; 83 fe 10
    jnc short 046feh                          ; 73 04
    mov byte [bp-004h], 001h                  ; c6 46 fc 01
    xor al, al                                ; 30 c0
    mov byte [bp-002h], al                    ; 88 46 fe
    mov byte [bp-006h], al                    ; 88 46 fa
    mov byte [bp-008h], al                    ; 88 46 f8
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 9a d1
    push si                                   ; 56
    movzx ax, byte [bp+008h]                  ; 0f b6 46 08
    push ax                                   ; 50
    push 004c4h                               ; 68 c4 04
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 c7 d1
    add sp, strict byte 00008h                ; 83 c4 08
    and si, strict byte 0000fh                ; 83 e6 0f
    cmp si, strict byte 00002h                ; 83 fe 02
    jc short 0473bh                           ; 72 0e
    jbe short 0474ah                          ; 76 1b
    cmp si, strict byte 00004h                ; 83 fe 04
    je short 04768h                           ; 74 34
    cmp si, strict byte 00003h                ; 83 fe 03
    je short 0475eh                           ; 74 25
    jmp short 04795h                          ; eb 5a
    cmp si, strict byte 00001h                ; 83 fe 01
    jne short 04795h                          ; 75 55
    xor al, al                                ; 30 c0
    mov byte [bp-002h], al                    ; 88 46 fe
    mov byte [bp-006h], al                    ; 88 46 fa
    jmp short 047adh                          ; eb 63
    mov dx, 00338h                            ; ba 38 03
    mov ax, word [bp-00ah]                    ; 8b 46 f6
    call 01600h                               ; e8 ad ce
    add AL, strict byte 080h                  ; 04 80
    mov byte [bp-002h], al                    ; 88 46 fe
    mov byte [bp-006h], 000h                  ; c6 46 fa 00
    jmp short 047adh                          ; eb 4f
    mov byte [bp-002h], 000h                  ; c6 46 fe 00
    mov byte [bp-006h], 001h                  ; c6 46 fa 01
    jmp short 04772h                          ; eb 0a
    mov byte [bp-008h], 001h                  ; c6 46 f8 01
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00
    je short 047adh                           ; 74 3b
    call 037d6h                               ; e8 61 f0
    mov bx, ax                                ; 89 c3
    test AL, strict byte 0ffh                 ; a8 ff
    je short 0479ch                           ; 74 21
    call 0464ah                               ; e8 cc fe
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc
    push ax                                   ; 50
    movzx bx, byte [bp-002h]                  ; 0f b6 5e fe
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa
    mov cx, strict word 00001h                ; b9 01 00
    call 045bah                               ; e8 25 fe
    xor ax, ax                                ; 31 c0
    xor dx, dx                                ; 31 d2
    jmp near 048bbh                           ; e9 1f 01
    mov dx, 0032eh                            ; ba 2e 03
    mov ax, word [bp-00ah]                    ; 8b 46 f6
    call 0161ch                               ; e8 77 ce
    mov di, ax                                ; 89 c7
    shr bx, 008h                              ; c1 eb 08
    mov byte [bp-002h], bl                    ; 88 5e fe
    cmp byte [bp-008h], 001h                  ; 80 7e f8 01
    jne near 04829h                           ; 0f 85 74 00
    xor si, si                                ; 31 f6
    mov ax, 0e200h                            ; b8 00 e2
    mov es, ax                                ; 8e c0
    cmp word [es:si], 0aa55h                  ; 26 81 3c 55 aa
    jne short 0477eh                          ; 75 bb
    mov cx, ax                                ; 89 c1
    mov si, word [es:si+01ah]                 ; 26 8b 74 1a
    cmp word [es:si+002h], 0506eh             ; 26 81 7c 02 6e 50
    jne short 0477eh                          ; 75 ad
    cmp word [es:si], 05024h                  ; 26 81 3c 24 50
    jne short 0477eh                          ; 75 a6
    mov di, word [es:si+00eh]                 ; 26 8b 7c 0e
    mov dx, word [es:di]                      ; 26 8b 15
    mov ax, word [es:di+002h]                 ; 26 8b 45 02
    cmp ax, 06568h                            ; 3d 68 65
    jne short 04807h                          ; 75 1f
    cmp dx, 07445h                            ; 81 fa 45 74
    jne short 04807h                          ; 75 19
    movzx bx, byte [bp-002h]                  ; 0f b6 5e fe
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa
    call 04572h                               ; e8 75 fd
    mov word [bp-00eh], strict word 00006h    ; c7 46 f2 06 00
    mov word [bp-00ch], cx                    ; 89 4e f4
    jmp short 04823h                          ; eb 1c
    movzx bx, byte [bp-002h]                  ; 0f b6 5e fe
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa
    call 04572h                               ; e8 5c fd
    sti                                       ; fb
    mov word [bp-00ch], cx                    ; 89 4e f4
    mov es, cx                                ; 8e c1
    mov ax, word [es:si+01ah]                 ; 26 8b 44 1a
    mov word [bp-00eh], ax                    ; 89 46 f2
    call far [bp-00eh]                        ; ff 5e f2
    jmp near 0477eh                           ; e9 55 ff
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00
    jne short 04855h                          ; 75 26
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00
    jne short 04855h                          ; 75 20
    mov di, 007c0h                            ; bf c0 07
    mov es, di                                ; 8e c7
    mov dl, byte [bp-002h]                    ; 8a 56 fe
    mov ax, 00201h                            ; b8 01 02
    mov DH, strict byte 000h                  ; b6 00
    mov cx, strict word 00001h                ; b9 01 00
    db  033h, 0dbh
    ; xor bx, bx                                ; 33 db
    int 013h                                  ; cd 13
    mov ax, strict word 00000h                ; b8 00 00
    sbb ax, strict byte 00000h                ; 83 d8 00
    test ax, ax                               ; 85 c0
    jne near 0477eh                           ; 0f 85 29 ff
    cmp byte [bp-002h], 000h                  ; 80 7e fe 00
    db  00fh, 094h, 0c1h
    ; sete cl                                   ; 0f 94 c1
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00
    je short 04864h                           ; 74 02
    mov CL, strict byte 001h                  ; b1 01
    xor dx, dx                                ; 31 d2
    mov ax, di                                ; 89 f8
    call 0161ch                               ; e8 b1 cd
    mov bx, ax                                ; 89 c3
    mov dx, strict word 00002h                ; ba 02 00
    mov ax, di                                ; 89 f8
    call 0161ch                               ; e8 a7 cd
    cmp bx, ax                                ; 39 c3
    je short 0488ah                           ; 74 11
    test cl, cl                               ; 84 c9
    jne short 048a0h                          ; 75 23
    mov dx, 001feh                            ; ba fe 01
    mov ax, di                                ; 89 f8
    call 0161ch                               ; e8 97 cd
    cmp ax, 0aa55h                            ; 3d 55 aa
    je short 048a0h                           ; 74 16
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc
    push ax                                   ; 50
    movzx bx, byte [bp-002h]                  ; 0f b6 5e fe
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa
    xor cx, cx                                ; 31 c9
    jmp near 04792h                           ; e9 f2 fe
    movzx bx, byte [bp-002h]                  ; 0f b6 5e fe
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa
    call 04572h                               ; e8 c3 fc
    movzx bx, byte [bp-002h]                  ; 0f b6 5e fe
    xor dx, dx                                ; 31 d2
    xor ax, ax                                ; 31 c0
    add ax, di                                ; 01 f8
    adc dx, bx                                ; 11 da
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
keyboard_panic_:                             ; 0xf48bf LB 0x11
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push ax                                   ; 50
    push 004e4h                               ; 68 e4 04
    push strict byte 00007h                   ; 6a 07
    call 018e9h                               ; e8 1e d0
    add sp, strict byte 00006h                ; 83 c4 06
    pop bp                                    ; 5d
    retn                                      ; c3
_keyboard_init:                              ; 0xf48d0 LB 0x27a
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov AL, strict byte 0aah                  ; b0 aa
    mov dx, strict word 00064h                ; ba 64 00
    out DX, AL                                ; ee
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 002h                 ; a8 02
    je short 048f3h                           ; 74 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 048f3h                          ; 76 08
    xor al, al                                ; 30 c0
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 048dch                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 048fch                          ; 75 05
    xor ax, ax                                ; 31 c0
    call 048bfh                               ; e8 c3 ff
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 04916h                          ; 75 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 04916h                          ; 76 08
    mov AL, strict byte 001h                  ; b0 01
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 048ffh                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 04920h                          ; 75 06
    mov ax, strict word 00001h                ; b8 01 00
    call 048bfh                               ; e8 9f ff
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp ax, strict word 00055h                ; 3d 55 00
    je short 04931h                           ; 74 06
    mov ax, 003dfh                            ; b8 df 03
    call 048bfh                               ; e8 8e ff
    mov AL, strict byte 0abh                  ; b0 ab
    mov dx, strict word 00064h                ; ba 64 00
    out DX, AL                                ; ee
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 002h                 ; a8 02
    je short 04951h                           ; 74 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 04951h                          ; 76 08
    mov AL, strict byte 010h                  ; b0 10
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 0493ah                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 0495bh                          ; 75 06
    mov ax, strict word 0000ah                ; b8 0a 00
    call 048bfh                               ; e8 64 ff
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 04975h                          ; 75 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 04975h                          ; 76 08
    mov AL, strict byte 011h                  ; b0 11
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 0495eh                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 0497fh                          ; 75 06
    mov ax, strict word 0000bh                ; b8 0b 00
    call 048bfh                               ; e8 40 ff
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test ax, ax                               ; 85 c0
    je short 0498fh                           ; 74 06
    mov ax, 003e0h                            ; b8 e0 03
    call 048bfh                               ; e8 30 ff
    mov AL, strict byte 0ffh                  ; b0 ff
    mov dx, strict word 00060h                ; ba 60 00
    out DX, AL                                ; ee
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 002h                 ; a8 02
    je short 049afh                           ; 74 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 049afh                          ; 76 08
    mov AL, strict byte 020h                  ; b0 20
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 04998h                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 049b9h                          ; 75 06
    mov ax, strict word 00014h                ; b8 14 00
    call 048bfh                               ; e8 06 ff
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 049d3h                          ; 75 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 049d3h                          ; 76 08
    mov AL, strict byte 021h                  ; b0 21
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 049bch                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 049ddh                          ; 75 06
    mov ax, strict word 00015h                ; b8 15 00
    call 048bfh                               ; e8 e2 fe
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp ax, 000fah                            ; 3d fa 00
    je short 049eeh                           ; 74 06
    mov ax, 003e1h                            ; b8 e1 03
    call 048bfh                               ; e8 d1 fe
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 04a08h                          ; 75 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 04a08h                          ; 76 08
    mov AL, strict byte 031h                  ; b0 31
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 049f1h                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 04a12h                          ; 75 06
    mov ax, strict word 0001fh                ; b8 1f 00
    call 048bfh                               ; e8 ad fe
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp ax, 000aah                            ; 3d aa 00
    je short 04a2bh                           ; 74 0e
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp ax, 000aah                            ; 3d aa 00
    je short 04a2bh                           ; 74 06
    mov ax, 003e2h                            ; b8 e2 03
    call 048bfh                               ; e8 94 fe
    mov AL, strict byte 0f5h                  ; b0 f5
    mov dx, strict word 00060h                ; ba 60 00
    out DX, AL                                ; ee
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 002h                 ; a8 02
    je short 04a4bh                           ; 74 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 04a4bh                          ; 76 08
    mov AL, strict byte 040h                  ; b0 40
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 04a34h                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 04a55h                          ; 75 06
    mov ax, strict word 00028h                ; b8 28 00
    call 048bfh                               ; e8 6a fe
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 04a6fh                          ; 75 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 04a6fh                          ; 76 08
    mov AL, strict byte 041h                  ; b0 41
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 04a58h                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 04a79h                          ; 75 06
    mov ax, strict word 00029h                ; b8 29 00
    call 048bfh                               ; e8 46 fe
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp ax, 000fah                            ; 3d fa 00
    je short 04a8ah                           ; 74 06
    mov ax, 003e3h                            ; b8 e3 03
    call 048bfh                               ; e8 35 fe
    mov AL, strict byte 060h                  ; b0 60
    mov dx, strict word 00064h                ; ba 64 00
    out DX, AL                                ; ee
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 002h                 ; a8 02
    je short 04aaah                           ; 74 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 04aaah                          ; 76 08
    mov AL, strict byte 050h                  ; b0 50
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 04a93h                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 04ab4h                          ; 75 06
    mov ax, strict word 00032h                ; b8 32 00
    call 048bfh                               ; e8 0b fe
    mov AL, strict byte 065h                  ; b0 65
    mov dx, strict word 00060h                ; ba 60 00
    out DX, AL                                ; ee
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 002h                 ; a8 02
    je short 04ad4h                           ; 74 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 04ad4h                          ; 76 08
    mov AL, strict byte 060h                  ; b0 60
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 04abdh                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 04adeh                          ; 75 06
    mov ax, strict word 0003ch                ; b8 3c 00
    call 048bfh                               ; e8 e1 fd
    mov AL, strict byte 0f4h                  ; b0 f4
    mov dx, strict word 00060h                ; ba 60 00
    out DX, AL                                ; ee
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 002h                 ; a8 02
    je short 04afeh                           ; 74 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 04afeh                          ; 76 08
    mov AL, strict byte 070h                  ; b0 70
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 04ae7h                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 04b08h                          ; 75 06
    mov ax, strict word 00046h                ; b8 46 00
    call 048bfh                               ; e8 b7 fd
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 04b22h                          ; 75 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 04b22h                          ; 76 08
    mov AL, strict byte 071h                  ; b0 71
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 04b0bh                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 04b2ch                          ; 75 06
    mov ax, strict word 00046h                ; b8 46 00
    call 048bfh                               ; e8 93 fd
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp ax, 000fah                            ; 3d fa 00
    je short 04b3dh                           ; 74 06
    mov ax, 003e4h                            ; b8 e4 03
    call 048bfh                               ; e8 82 fd
    mov AL, strict byte 0a8h                  ; b0 a8
    mov dx, strict word 00064h                ; ba 64 00
    out DX, AL                                ; ee
    xor ax, ax                                ; 31 c0
    call 05d9ah                               ; e8 52 12
    pop bp                                    ; 5d
    retn                                      ; c3
enqueue_key_:                                ; 0xf4b4a LB 0x90
    push bx                                   ; 53
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    enter 00002h, 000h                        ; c8 02 00 00
    mov byte [bp-002h], al                    ; 88 46 fe
    mov bl, dl                                ; 88 d3
    mov dx, strict word 0001ah                ; ba 1a 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 bc ca
    mov di, ax                                ; 89 c7
    mov dx, strict word 0001ch                ; ba 1c 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 b1 ca
    mov si, ax                                ; 89 c6
    lea cx, [si+002h]                         ; 8d 4c 02
    cmp cx, strict byte 0003eh                ; 83 f9 3e
    jc short 04b78h                           ; 72 03
    mov cx, strict word 0001eh                ; b9 1e 00
    cmp cx, di                                ; 39 f9
    jne short 04b80h                          ; 75 04
    xor ax, ax                                ; 31 c0
    jmp short 04ba5h                          ; eb 25
    xor bh, bh                                ; 30 ff
    mov dx, si                                ; 89 f2
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 84 ca
    movzx bx, byte [bp-002h]                  ; 0f b6 5e fe
    lea dx, [si+001h]                         ; 8d 54 01
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 77 ca
    mov bx, cx                                ; 89 cb
    mov dx, strict word 0001ch                ; ba 1c 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0162ah                               ; e8 88 ca
    mov ax, strict word 00001h                ; b8 01 00
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
    db  0c6h, 0c5h, 0bah
    ; mov ch, 0bah                              ; c6 c5 ba
    mov ax, 0aab6h                            ; b8 b6 aa
    popfw                                     ; 9d
    push bx                                   ; 53
    inc si                                    ; 46
    inc bp                                    ; 45
    cmp bh, byte [bx+si]                      ; 3a 38
    sub bl, byte [ss:di]                      ; 36 2a 1d
    adc cl, byte [bp-061h]                    ; 12 4e 9f
    dec sp                                    ; 4c
    outsb                                     ; 6e
    dec sp                                    ; 4c
    outsb                                     ; 6e
    dec sp                                    ; 4c
    and word [di+047h], cx                    ; 21 4d 47
    dec sp                                    ; 4c
    xchg bx, ax                               ; 93
    dec bp                                    ; 4d
    loop 04c17h                               ; e2 4d
    add ax, 0e04eh                            ; 05 4e e0
    dec sp                                    ; 4c
    outsb                                     ; 6e
    dec sp                                    ; 4c
    outsb                                     ; 6e
    dec sp                                    ; 4c
    pop dx                                    ; 5a
    dec bp                                    ; 4d
    pushaw                                    ; 60
    dec sp                                    ; 4c
    retn                                      ; c3
    dec bp                                    ; 4d
    db  0feh
    dec bp                                    ; 4d
_int09_function:                             ; 0xf4bda LB 0x358
    push si                                   ; 56
    push di                                   ; 57
    enter 0000ch, 000h                        ; c8 0c 00 00
    mov al, byte [bp+018h]                    ; 8a 46 18
    mov byte [bp-006h], al                    ; 88 46 fa
    test al, al                               ; 84 c0
    jne short 04c03h                          ; 75 19
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 b9 cc
    push 004f7h                               ; 68 f7 04
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 ec cc
    add sp, strict byte 00004h                ; 83 c4 04
    jmp near 04f2eh                           ; e9 2b 03
    mov dx, strict word 00017h                ; ba 17 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 f4 c9
    mov byte [bp-00ah], al                    ; 88 46 f6
    mov bl, al                                ; 88 c3
    mov dx, strict word 00018h                ; ba 18 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 e6 c9
    mov byte [bp-00ch], al                    ; 88 46 f4
    mov byte [bp-008h], al                    ; 88 46 f8
    mov dx, 00096h                            ; ba 96 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 d7 c9
    mov byte [bp-004h], al                    ; 88 46 fc
    mov byte [bp-002h], al                    ; 88 46 fe
    mov al, byte [bp-006h]                    ; 8a 46 fa
    push CS                                   ; 0e
    pop ES                                    ; 07
    mov cx, strict word 00010h                ; b9 10 00
    mov di, 04babh                            ; bf ab 4b
    repne scasb                               ; f2 ae
    sal cx, 1                                 ; d1 e1
    mov di, cx                                ; 89 cf
    mov ax, word [cs:di+04bbah]               ; 2e 8b 85 ba 4b
    jmp ax                                    ; ff e0
    xor bl, 040h                              ; 80 f3 40
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00017h                ; ba 17 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 b9 c9
    or byte [bp-008h], 040h                   ; 80 4e f8 40
    movzx bx, byte [bp-008h]                  ; 0f b6 5e f8
    jmp near 04dd6h                           ; e9 76 01
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    and AL, strict byte 0bfh                  ; 24 bf
    mov byte [bp-008h], al                    ; 88 46 f8
    movzx bx, al                              ; 0f b6 d8
    jmp near 04dd6h                           ; e9 68 01
    test byte [bp-002h], 002h                 ; f6 46 fe 02
    jne near 04f10h                           ; 0f 85 9a 02
    mov al, byte [bp-006h]                    ; 8a 46 fa
    and AL, strict byte 07fh                  ; 24 7f
    cmp AL, strict byte 02ah                  ; 3c 2a
    db  00fh, 094h, 0c0h
    ; sete al                                   ; 0f 94 c0
    xor ah, ah                                ; 30 e4
    inc ax                                    ; 40
    test byte [bp-006h], 080h                 ; f6 46 fa 80
    je short 04c8fh                           ; 74 06
    not al                                    ; f6 d0
    and bl, al                                ; 20 c3
    jmp short 04c91h                          ; eb 02
    or bl, al                                 ; 08 c3
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00017h                ; ba 17 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 72 c9
    jmp near 04f10h                           ; e9 71 02
    test byte [bp-004h], 001h                 ; f6 46 fc 01
    jne near 04f10h                           ; 0f 85 69 02
    or bl, 004h                               ; 80 cb 04
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00017h                ; ba 17 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 59 c9
    mov al, byte [bp-004h]                    ; 8a 46 fc
    test AL, strict byte 002h                 ; a8 02
    je short 04cc9h                           ; 74 0d
    or AL, strict byte 004h                   ; 0c 04
    mov byte [bp-002h], al                    ; 88 46 fe
    movzx bx, al                              ; 0f b6 d8
    mov dx, 00096h                            ; ba 96 00
    jmp short 04cd7h                          ; eb 0e
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    or AL, strict byte 001h                   ; 0c 01
    mov byte [bp-008h], al                    ; 88 46 f8
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 00018h                ; ba 18 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 31 c9
    jmp near 04f10h                           ; e9 30 02
    test byte [bp-004h], 001h                 ; f6 46 fc 01
    jne near 04f10h                           ; 0f 85 28 02
    and bl, 0fbh                              ; 80 e3 fb
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00017h                ; ba 17 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 18 c9
    mov al, byte [bp-004h]                    ; 8a 46 fc
    test AL, strict byte 002h                 ; a8 02
    je short 04d0ah                           ; 74 0d
    and AL, strict byte 0fbh                  ; 24 fb
    mov byte [bp-002h], al                    ; 88 46 fe
    movzx bx, al                              ; 0f b6 d8
    mov dx, 00096h                            ; ba 96 00
    jmp short 04d18h                          ; eb 0e
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    and AL, strict byte 0feh                  ; 24 fe
    mov byte [bp-008h], al                    ; 88 46 f8
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 00018h                ; ba 18 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 f0 c8
    jmp near 04f10h                           ; e9 ef 01
    or bl, 008h                               ; 80 cb 08
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00017h                ; ba 17 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 df c8
    mov al, byte [bp-004h]                    ; 8a 46 fc
    test AL, strict byte 002h                 ; a8 02
    je short 04d43h                           ; 74 0d
    or AL, strict byte 008h                   ; 0c 08
    mov byte [bp-002h], al                    ; 88 46 fe
    movzx bx, al                              ; 0f b6 d8
    mov dx, 00096h                            ; ba 96 00
    jmp short 04d51h                          ; eb 0e
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    or AL, strict byte 002h                   ; 0c 02
    mov byte [bp-008h], al                    ; 88 46 f8
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 00018h                ; ba 18 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 b7 c8
    jmp near 04f10h                           ; e9 b6 01
    and bl, 0f7h                              ; 80 e3 f7
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00017h                ; ba 17 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 a6 c8
    mov al, byte [bp-004h]                    ; 8a 46 fc
    test AL, strict byte 002h                 ; a8 02
    je short 04d7ch                           ; 74 0d
    and AL, strict byte 0f7h                  ; 24 f7
    mov byte [bp-002h], al                    ; 88 46 fe
    movzx bx, al                              ; 0f b6 d8
    mov dx, 00096h                            ; ba 96 00
    jmp short 04d8ah                          ; eb 0e
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    and AL, strict byte 0fdh                  ; 24 fd
    mov byte [bp-008h], al                    ; 88 46 f8
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 00018h                ; ba 18 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 7e c8
    jmp near 04f10h                           ; e9 7d 01
    test byte [bp-004h], 003h                 ; f6 46 fc 03
    jne near 04f10h                           ; 0f 85 75 01
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    or AL, strict byte 020h                   ; 0c 20
    mov byte [bp-008h], al                    ; 88 46 f8
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 00018h                ; ba 18 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 5f c8
    mov bl, byte [bp-00ah]                    ; 8a 5e f6
    xor bl, 020h                              ; 80 f3 20
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00017h                ; ba 17 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 4e c8
    jmp near 04f10h                           ; e9 4d 01
    test byte [bp-004h], 003h                 ; f6 46 fc 03
    jne near 04f10h                           ; 0f 85 45 01
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    and AL, strict byte 0dfh                  ; 24 df
    mov byte [bp-008h], al                    ; 88 46 f8
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 00018h                ; ba 18 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 2f c8
    jmp near 04f10h                           ; e9 2e 01
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    or AL, strict byte 010h                   ; 0c 10
    mov byte [bp-008h], al                    ; 88 46 f8
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 00018h                ; ba 18 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 18 c8
    mov bl, byte [bp-00ah]                    ; 8a 5e f6
    xor bl, 010h                              ; 80 f3 10
    jmp short 04db5h                          ; eb b7
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    and AL, strict byte 0efh                  ; 24 ef
    jmp short 04dd0h                          ; eb cb
    mov al, bl                                ; 88 d8
    and AL, strict byte 00ch                  ; 24 0c
    cmp AL, strict byte 00ch                  ; 3c 0c
    jne short 04e12h                          ; 75 05
    jmp far 0f000h:0e05bh                     ; ea 5b e0 00 f0
    test byte [bp-006h], 080h                 ; f6 46 fa 80
    jne near 04f10h                           ; 0f 85 f6 00
    cmp byte [bp-006h], 058h                  ; 80 7e fa 58
    jbe short 04e3eh                          ; 76 1e
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 83 ca
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa
    push ax                                   ; 50
    push 00511h                               ; 68 11 05
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 b1 ca
    add sp, strict byte 00006h                ; 83 c4 06
    jmp near 04f2eh                           ; e9 f0 00
    test bl, 008h                             ; f6 c3 08
    je short 04e55h                           ; 74 12
    movzx si, byte [bp-006h]                  ; 0f b6 76 fa
    imul si, si, strict byte 0000ah           ; 6b f6 0a
    mov dl, byte [si+00d86h]                  ; 8a 94 86 0d
    mov ax, word [si+00d86h]                  ; 8b 84 86 0d
    jmp near 04ee1h                           ; e9 8c 00
    test bl, 004h                             ; f6 c3 04
    je short 04e6ch                           ; 74 12
    movzx si, byte [bp-006h]                  ; 0f b6 76 fa
    imul si, si, strict byte 0000ah           ; 6b f6 0a
    mov dl, byte [si+00d84h]                  ; 8a 94 84 0d
    mov ax, word [si+00d84h]                  ; 8b 84 84 0d
    jmp near 04ee1h                           ; e9 75 00
    mov al, byte [bp-002h]                    ; 8a 46 fe
    and AL, strict byte 002h                  ; 24 02
    test al, al                               ; 84 c0
    jbe short 04e8ah                          ; 76 15
    mov al, byte [bp-006h]                    ; 8a 46 fa
    cmp AL, strict byte 047h                  ; 3c 47
    jc short 04e8ah                           ; 72 0e
    cmp AL, strict byte 053h                  ; 3c 53
    jnbe short 04e8ah                         ; 77 0a
    mov DL, strict byte 0e0h                  ; b2 e0
    movzx si, al                              ; 0f b6 f0
    imul si, si, strict byte 0000ah           ; 6b f6 0a
    jmp short 04eddh                          ; eb 53
    test bl, 003h                             ; f6 c3 03
    je short 04ebch                           ; 74 2d
    movzx si, byte [bp-006h]                  ; 0f b6 76 fa
    imul si, si, strict byte 0000ah           ; 6b f6 0a
    movzx ax, byte [si+00d88h]                ; 0f b6 84 88 0d
    movzx dx, bl                              ; 0f b6 d3
    test dx, ax                               ; 85 c2
    je short 04each                           ; 74 0a
    mov dl, byte [si+00d80h]                  ; 8a 94 80 0d
    mov ax, word [si+00d80h]                  ; 8b 84 80 0d
    jmp short 04eb4h                          ; eb 08
    mov dl, byte [si+00d82h]                  ; 8a 94 82 0d
    mov ax, word [si+00d82h]                  ; 8b 84 82 0d
    shr ax, 008h                              ; c1 e8 08
    mov byte [bp-006h], al                    ; 88 46 fa
    jmp short 04ee7h                          ; eb 2b
    movzx si, byte [bp-006h]                  ; 0f b6 76 fa
    imul si, si, strict byte 0000ah           ; 6b f6 0a
    movzx ax, byte [si+00d88h]                ; 0f b6 84 88 0d
    movzx dx, bl                              ; 0f b6 d3
    test dx, ax                               ; 85 c2
    je short 04ed9h                           ; 74 0a
    mov dl, byte [si+00d82h]                  ; 8a 94 82 0d
    mov ax, word [si+00d82h]                  ; 8b 84 82 0d
    jmp short 04ee1h                          ; eb 08
    mov dl, byte [si+00d80h]                  ; 8a 94 80 0d
    mov ax, word [si+00d80h]                  ; 8b 84 80 0d
    shr ax, 008h                              ; c1 e8 08
    mov byte [bp-006h], al                    ; 88 46 fa
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00
    jne short 04f07h                          ; 75 1a
    test dl, dl                               ; 84 d2
    jne short 04f07h                          ; 75 16
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 b2 c9
    push 00548h                               ; 68 48 05
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 e5 c9
    add sp, strict byte 00004h                ; 83 c4 04
    xor dh, dh                                ; 30 f6
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa
    call 04b4ah                               ; e8 3a fc
    mov al, byte [bp-006h]                    ; 8a 46 fa
    and AL, strict byte 07fh                  ; 24 7f
    cmp AL, strict byte 01dh                  ; 3c 1d
    je short 04f1dh                           ; 74 04
    and byte [bp-002h], 0feh                  ; 80 66 fe fe
    and byte [bp-002h], 0fdh                  ; 80 66 fe fd
    movzx bx, byte [bp-002h]                  ; 0f b6 5e fe
    mov dx, 00096h                            ; ba 96 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 e0 c6
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
dequeue_key_:                                ; 0xf4f32 LB 0x90
    push si                                   ; 56
    push di                                   ; 57
    enter 00004h, 000h                        ; c8 04 00 00
    mov di, ax                                ; 89 c7
    mov word [bp-002h], dx                    ; 89 56 fe
    mov si, bx                                ; 89 de
    mov word [bp-004h], cx                    ; 89 4e fc
    mov dx, strict word 0001ah                ; ba 1a 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 d1 c6
    mov bx, ax                                ; 89 c3
    mov dx, strict word 0001ch                ; ba 1c 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 c6 c6
    cmp bx, ax                                ; 39 c3
    je short 04f97h                           ; 74 3d
    mov dx, bx                                ; 89 da
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 9e c6
    mov cl, al                                ; 88 c1
    lea dx, [bx+001h]                         ; 8d 57 01
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 93 c6
    mov es, [bp-004h]                         ; 8e 46 fc
    mov byte [es:si], cl                      ; 26 88 0c
    mov es, [bp-002h]                         ; 8e 46 fe
    mov byte [es:di], al                      ; 26 88 05
    cmp word [bp+008h], strict byte 00000h    ; 83 7e 08 00
    je short 04f92h                           ; 74 13
    inc bx                                    ; 43
    inc bx                                    ; 43
    cmp bx, strict byte 0003eh                ; 83 fb 3e
    jc short 04f89h                           ; 72 03
    mov bx, strict word 0001eh                ; bb 1e 00
    mov dx, strict word 0001ah                ; ba 1a 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0162ah                               ; e8 98 c6
    mov ax, strict word 00001h                ; b8 01 00
    jmp short 04f99h                          ; eb 02
    xor ax, ax                                ; 31 c0
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn 00002h                               ; c2 02 00
    mov byte [01292h], AL                     ; a2 92 12
    adc word [bx+si], dx                      ; 11 10
    or cl, byte [bx+di]                       ; 0a 09
    add ax, 00102h                            ; 05 02 01
    add byte [bp+052h], bl                    ; 00 5e 52
    je short 04ffeh                           ; 74 50
    mov bx, 00750h                            ; bb 50 07
    push cx                                   ; 51
    pop SS                                    ; 17
    push cx                                   ; 51
    inc cx                                    ; 41
    push cx                                   ; 51
    dec dx                                    ; 4a
    push cx                                   ; 51
    mov bx, 0ec51h                            ; bb 51 ec
    push cx                                   ; 51
    sbb word [bp+si+053h], dx                 ; 19 52 53
    push dx                                   ; 52
    db  0a1h
    push dx                                   ; 52
_int16_function:                             ; 0xf4fc2 LB 0x2e2
    push di                                   ; 57
    enter 00006h, 000h                        ; c8 06 00 00
    mov dx, strict word 00017h                ; ba 17 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 30 c6
    mov cl, al                                ; 88 c1
    mov bh, al                                ; 88 c7
    mov dx, 00097h                            ; ba 97 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 23 c6
    mov bl, al                                ; 88 c3
    movzx dx, cl                              ; 0f b6 d1
    sar dx, 004h                              ; c1 fa 04
    and dl, 007h                              ; 80 e2 07
    and AL, strict byte 007h                  ; 24 07
    xor ah, ah                                ; 30 e4
    xor al, dl                                ; 30 d0
    test ax, ax                               ; 85 c0
    je short 05052h                           ; 74 60
    cli                                       ; fa
    mov AL, strict byte 0edh                  ; b0 ed
    mov dx, strict word 00060h                ; ba 60 00
    out DX, AL                                ; ee
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 0500bh                          ; 75 08
    mov AL, strict byte 021h                  ; b0 21
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 04ff9h                          ; eb ee
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp ax, 000fah                            ; 3d fa 00
    jne short 05051h                          ; 75 3b
    and bl, 0f8h                              ; 80 e3 f8
    movzx ax, bh                              ; 0f b6 c7
    sar ax, 004h                              ; c1 f8 04
    and ax, strict word 00007h                ; 25 07 00
    movzx cx, bl                              ; 0f b6 cb
    or cx, ax                                 ; 09 c1
    mov bl, cl                                ; 88 cb
    mov al, cl                                ; 88 c8
    and AL, strict byte 007h                  ; 24 07
    out DX, AL                                ; ee
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 05040h                          ; 75 08
    mov AL, strict byte 021h                  ; b0 21
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 0502eh                          ; eb ee
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    xor bh, bh                                ; 30 ff
    mov dx, 00097h                            ; ba 97 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 bd c5
    sti                                       ; fb
    mov ax, word [bp+014h]                    ; 8b 46 14
    shr ax, 008h                              ; c1 e8 08
    cmp ax, 000a2h                            ; 3d a2 00
    jnbe near 0525eh                          ; 0f 87 ff 01
    push CS                                   ; 0e
    pop ES                                    ; 07
    mov cx, strict word 0000ch                ; b9 0c 00
    mov di, 04f9fh                            ; bf 9f 4f
    repne scasb                               ; f2 ae
    sal cx, 1                                 ; d1 e1
    mov di, cx                                ; 89 cf
    mov ax, word [cs:di+04faah]               ; 2e 8b 85 aa 4f
    jmp ax                                    ; ff e0
    push strict byte 00001h                   ; 6a 01
    mov cx, ss                                ; 8c d1
    lea bx, [bp-006h]                         ; 8d 5e fa
    mov dx, ss                                ; 8c d2
    lea ax, [bp-004h]                         ; 8d 46 fc
    call 04f32h                               ; e8 af fe
    test ax, ax                               ; 85 c0
    jne short 05092h                          ; 75 0b
    push 0057fh                               ; 68 7f 05
    push strict byte 00007h                   ; 6a 07
    call 018e9h                               ; e8 5a c8
    add sp, strict byte 00004h                ; 83 c4 04
    cmp byte [bp-004h], 000h                  ; 80 7e fc 00
    je short 0509eh                           ; 74 06
    cmp byte [bp-006h], 0f0h                  ; 80 7e fa f0
    je short 050a4h                           ; 74 06
    cmp byte [bp-006h], 0e0h                  ; 80 7e fa e0
    jne short 050a8h                          ; 75 04
    mov byte [bp-006h], 000h                  ; c6 46 fa 00
    movzx dx, byte [bp-004h]                  ; 0f b6 56 fc
    sal dx, 008h                              ; c1 e2 08
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa
    or dx, ax                                 ; 09 c2
    mov word [bp+014h], dx                    ; 89 56 14
    leave                                     ; c9
    pop di                                    ; 5f
    retn                                      ; c3
    or word [bp+01eh], 00200h                 ; 81 4e 1e 00 02
    push strict byte 00000h                   ; 6a 00
    mov cx, ss                                ; 8c d1
    lea bx, [bp-006h]                         ; 8d 5e fa
    mov dx, ss                                ; 8c d2
    lea ax, [bp-004h]                         ; 8d 46 fc
    call 04f32h                               ; e8 63 fe
    test ax, ax                               ; 85 c0
    jne short 050dah                          ; 75 07
    or word [bp+01eh], strict byte 00040h     ; 83 4e 1e 40
    leave                                     ; c9
    pop di                                    ; 5f
    retn                                      ; c3
    cmp byte [bp-004h], 000h                  ; 80 7e fc 00
    je short 050e6h                           ; 74 06
    cmp byte [bp-006h], 0f0h                  ; 80 7e fa f0
    je short 050ech                           ; 74 06
    cmp byte [bp-006h], 0e0h                  ; 80 7e fa e0
    jne short 050f0h                          ; 75 04
    mov byte [bp-006h], 000h                  ; c6 46 fa 00
    movzx dx, byte [bp-004h]                  ; 0f b6 56 fc
    sal dx, 008h                              ; c1 e2 08
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa
    or dx, ax                                 ; 09 c2
    mov word [bp+014h], dx                    ; 89 56 14
    and word [bp+01eh], strict byte 0ffbfh    ; 83 66 1e bf
    leave                                     ; c9
    pop di                                    ; 5f
    retn                                      ; c3
    mov dx, strict word 00017h                ; ba 17 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 f0 c4
    mov dx, word [bp+014h]                    ; 8b 56 14
    mov dl, al                                ; 88 c2
    jmp short 050b5h                          ; eb 9e
    mov al, byte [bp+012h]                    ; 8a 46 12
    movzx dx, al                              ; 0f b6 d0
    mov ax, word [bp+012h]                    ; 8b 46 12
    shr ax, 008h                              ; c1 e8 08
    xor ah, ah                                ; 30 e4
    call 04b4ah                               ; e8 22 fa
    test ax, ax                               ; 85 c0
    jne short 05139h                          ; 75 0d
    mov ax, word [bp+014h]                    ; 8b 46 14
    xor al, al                                ; 30 c0
    or AL, strict byte 001h                   ; 0c 01
    mov word [bp+014h], ax                    ; 89 46 14
    leave                                     ; c9
    pop di                                    ; 5f
    retn                                      ; c3
    and word [bp+014h], 0ff00h                ; 81 66 14 00 ff
    leave                                     ; c9
    pop di                                    ; 5f
    retn                                      ; c3
    mov ax, word [bp+014h]                    ; 8b 46 14
    xor al, al                                ; 30 c0
    or AL, strict byte 030h                   ; 0c 30
    jmp short 05133h                          ; eb e9
    mov byte [bp-002h], 002h                  ; c6 46 fe 02
    xor cx, cx                                ; 31 c9
    cli                                       ; fa
    mov AL, strict byte 0f2h                  ; b0 f2
    mov dx, strict word 00060h                ; ba 60 00
    out DX, AL                                ; ee
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 05171h                          ; 75 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 05171h                          ; 76 08
    mov dx, 00080h                            ; ba 80 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    jmp short 0515ah                          ; eb e9
    test bx, bx                               ; 85 db
    jbe short 051b5h                          ; 76 40
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp ax, 000fah                            ; 3d fa 00
    jne short 051b5h                          ; 75 35
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 0519ah                          ; 75 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 0519ah                          ; 76 08
    mov dx, 00080h                            ; ba 80 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    jmp short 05183h                          ; eb e9
    test bx, bx                               ; 85 db
    jbe short 051ach                          ; 76 0e
    shr cx, 008h                              ; c1 e9 08
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    sal ax, 008h                              ; c1 e0 08
    or cx, ax                                 ; 09 c1
    dec byte [bp-002h]                        ; fe 4e fe
    cmp byte [bp-002h], 000h                  ; 80 7e fe 00
    jnbe short 05180h                         ; 77 cb
    mov word [bp+00eh], cx                    ; 89 4e 0e
    leave                                     ; c9
    pop di                                    ; 5f
    retn                                      ; c3
    push strict byte 00001h                   ; 6a 01
    mov cx, ss                                ; 8c d1
    lea bx, [bp-006h]                         ; 8d 5e fa
    mov dx, ss                                ; 8c d2
    lea ax, [bp-004h]                         ; 8d 46 fc
    call 04f32h                               ; e8 68 fd
    test ax, ax                               ; 85 c0
    jne short 051d9h                          ; 75 0b
    push 0057fh                               ; 68 7f 05
    push strict byte 00007h                   ; 6a 07
    call 018e9h                               ; e8 13 c7
    add sp, strict byte 00004h                ; 83 c4 04
    cmp byte [bp-004h], 000h                  ; 80 7e fc 00
    je near 050a8h                            ; 0f 84 c7 fe
    cmp byte [bp-006h], 0f0h                  ; 80 7e fa f0
    je near 050a4h                            ; 0f 84 bb fe
    jmp near 050a8h                           ; e9 bc fe
    or word [bp+01eh], 00200h                 ; 81 4e 1e 00 02
    push strict byte 00000h                   ; 6a 00
    mov cx, ss                                ; 8c d1
    lea bx, [bp-006h]                         ; 8d 5e fa
    mov dx, ss                                ; 8c d2
    lea ax, [bp-004h]                         ; 8d 46 fc
    call 04f32h                               ; e8 32 fd
    test ax, ax                               ; 85 c0
    je near 050d3h                            ; 0f 84 cd fe
    cmp byte [bp-004h], 000h                  ; 80 7e fc 00
    je near 050f0h                            ; 0f 84 e2 fe
    cmp byte [bp-006h], 0f0h                  ; 80 7e fa f0
    je near 050ech                            ; 0f 84 d6 fe
    jmp near 050f0h                           ; e9 d7 fe
    mov dx, strict word 00017h                ; ba 17 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 de c3
    mov dx, word [bp+014h]                    ; 8b 56 14
    mov dl, al                                ; 88 c2
    mov word [bp+014h], dx                    ; 89 56 14
    mov dx, strict word 00018h                ; ba 18 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 cd c3
    mov bh, al                                ; 88 c7
    and bh, 073h                              ; 80 e7 73
    mov dx, 00096h                            ; ba 96 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 bf c3
    and AL, strict byte 00ch                  ; 24 0c
    or bh, al                                 ; 08 c7
    mov dx, word [bp+014h]                    ; 8b 56 14
    xor dh, dh                                ; 30 f6
    movzx ax, bh                              ; 0f b6 c7
    sal ax, 008h                              ; c1 e0 08
    jmp near 050b3h                           ; e9 60 fe
    mov ax, word [bp+014h]                    ; 8b 46 14
    xor ah, ah                                ; 30 e4
    or ah, 080h                               ; 80 cc 80
    jmp near 05133h                           ; e9 d5 fe
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 45 c6
    mov ax, word [bp+014h]                    ; 8b 46 14
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 005a3h                               ; 68 a3 05
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 71 c6
    add sp, strict byte 00006h                ; 83 c4 06
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 28 c6
    mov ax, word [bp+010h]                    ; 8b 46 10
    push ax                                   ; 50
    mov ax, word [bp+012h]                    ; 8b 46 12
    push ax                                   ; 50
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    push ax                                   ; 50
    mov ax, word [bp+014h]                    ; 8b 46 14
    push ax                                   ; 50
    push 005cbh                               ; 68 cb 05
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 4b c6
    add sp, strict byte 0000ch                ; 83 c4 0c
    leave                                     ; c9
    pop di                                    ; 5f
    retn                                      ; c3
set_geom_lba_:                               ; 0xf52a4 LB 0x9e
    push si                                   ; 56
    push di                                   ; 57
    enter 00004h, 000h                        ; c8 04 00 00
    mov si, ax                                ; 89 c6
    mov es, dx                                ; 8e c2
    mov word [bp-004h], bx                    ; 89 5e fc
    mov word [bp-002h], cx                    ; 89 4e fe
    xor ax, ax                                ; 31 c0
    mov dx, strict word 0007eh                ; ba 7e 00
    mov di, 000ffh                            ; bf ff 00
    xor bx, bx                                ; 31 db
    jmp short 052c5h                          ; eb 05
    cmp bx, strict byte 00004h                ; 83 fb 04
    jnl short 052dbh                          ; 7d 16
    cmp dx, word [bp-002h]                    ; 3b 56 fe
    jnbe short 052d1h                         ; 77 07
    jne short 052d4h                          ; 75 08
    cmp ax, word [bp-004h]                    ; 3b 46 fc
    jc short 052d4h                           ; 72 03
    inc di                                    ; 47
    shr di, 1                                 ; d1 ef
    shr dx, 1                                 ; d1 ea
    rcr ax, 1                                 ; d1 d8
    inc bx                                    ; 43
    jmp short 052c0h                          ; eb e5
    mov ax, di                                ; 89 f8
    xor dx, dx                                ; 31 d2
    mov bx, strict word 0003fh                ; bb 3f 00
    xor cx, cx                                ; 31 c9
    call 08fe9h                               ; e8 02 3d
    mov bx, ax                                ; 89 c3
    mov cx, dx                                ; 89 d1
    mov ax, word [bp-004h]                    ; 8b 46 fc
    mov dx, word [bp-002h]                    ; 8b 56 fe
    call 08fb0h                               ; e8 bc 3c
    mov word [es:si+002h], ax                 ; 26 89 44 02
    cmp ax, 00400h                            ; 3d 00 04
    jbe short 05303h                          ; 76 06
    mov word [es:si+002h], 00400h             ; 26 c7 44 02 00 04
    mov word [es:si], di                      ; 26 89 3c
    mov word [es:si+004h], strict word 0003fh ; 26 c7 44 04 3f 00
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
    retf 0ee53h                               ; ca 53 ee
    push bx                                   ; 53
    sbb dx, word [si+01bh]                    ; 1b 54 1b
    push sp                                   ; 54
    sbb dx, word [si-00dh]                    ; 1b 54 f3
    push bp                                   ; 55
    and word [bx+021h], dx                    ; 21 57 21
    push di                                   ; 57
    or AL, strict byte 056h                   ; 0c 56
    db  0feh
    push si                                   ; 56
    and word [bx+021h], dx                    ; 21 57 21
    push di                                   ; 57
    db  0feh
    push si                                   ; 56
    db  0feh
    push si                                   ; 56
    and word [bx+021h], dx                    ; 21 57 21
    push di                                   ; 57
    db  082h, 056h, 0feh, 056h
    ; adc byte [bp-002h], 056h                  ; 82 56 fe 56
    and word [bx+021h], dx                    ; 21 57 21
    push di                                   ; 57
    db  0feh
    push si                                   ; 56
    mov DL, strict byte 056h                  ; b2 56
    and word [bx+021h], dx                    ; 21 57 21
    push di                                   ; 57
    db  021h
    push di                                   ; 57
_int13_harddisk:                             ; 0xf5342 LB 0x43c
    enter 00010h, 000h                        ; c8 10 00 00
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 cd c2
    mov si, 00122h                            ; be 22 01
    mov word [bp-004h], ax                    ; 89 46 fc
    xor bx, bx                                ; 31 db
    mov dx, 0008eh                            ; ba 8e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 ae c2
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    xor ah, ah                                ; 30 e4
    cmp ax, 00080h                            ; 3d 80 00
    jc short 0536fh                           ; 72 05
    cmp ax, 00090h                            ; 3d 90 00
    jc short 0538dh                           ; 72 1e
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 005eeh                               ; 68 ee 05
    push 005fdh                               ; 68 fd 05
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 62 c5
    add sp, strict byte 0000ah                ; 83 c4 0a
    jmp near 0573ch                           ; e9 af 03
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    xor ah, ah                                ; 30 e4
    mov es, [bp-004h]                         ; 8e 46 fc
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    mov dl, byte [es:bx+0011fh]               ; 26 8a 97 1f 01
    mov byte [bp-002h], dl                    ; 88 56 fe
    cmp dl, 010h                              ; 80 fa 10
    jc short 053b6h                           ; 72 10
    push ax                                   ; 50
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 005eeh                               ; 68 ee 05
    push 00628h                               ; 68 28 06
    jmp short 05382h                          ; eb cc
    mov bx, word [bp+016h]                    ; 8b 5e 16
    shr bx, 008h                              ; c1 eb 08
    cmp bx, strict byte 00018h                ; 83 fb 18
    jnbe near 05721h                          ; 0f 87 5e 03
    add bx, bx                                ; 01 db
    jmp word [cs:bx+05310h]                   ; 2e ff a7 10 53
    cmp byte [bp-002h], 008h                  ; 80 7e fe 08
    jnc near 053d9h                           ; 0f 83 07 00
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    call 01be6h                               ; e8 0d c8
    mov byte [bp+017h], 000h                  ; c6 46 17 00
    xor bx, bx                                ; 31 db
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 26 c2
    and byte [bp+01ch], 0feh                  ; 80 66 1c fe
    leave                                     ; c9
    retn                                      ; c3
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 09 c2
    mov cl, al                                ; 88 c1
    mov dx, word [bp+016h]                    ; 8b 56 16
    xor dh, dh                                ; 30 f6
    movzx ax, cl                              ; 0f b6 c1
    sal ax, 008h                              ; c1 e0 08
    or dx, ax                                 ; 09 c2
    mov word [bp+016h], dx                    ; 89 56 16
    xor bx, bx                                ; 31 db
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 fa c1
    test cl, cl                               ; 84 c9
    je short 053ddh                           ; 74 c5
    jmp near 05758h                           ; e9 3d 03
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    mov word [bp-00ch], ax                    ; 89 46 f4
    mov di, word [bp+014h]                    ; 8b 7e 14
    shr di, 008h                              ; c1 ef 08
    mov ax, word [bp+014h]                    ; 8b 46 14
    xor ah, ah                                ; 30 e4
    sal ax, 002h                              ; c1 e0 02
    xor al, al                                ; 30 c0
    and ah, 003h                              ; 80 e4 03
    or di, ax                                 ; 09 c7
    mov ax, word [bp+014h]                    ; 8b 46 14
    and ax, strict word 0003fh                ; 25 3f 00
    mov word [bp-006h], ax                    ; 89 46 fa
    mov ax, word [bp+012h]                    ; 8b 46 12
    shr ax, 008h                              ; c1 e8 08
    mov word [bp-008h], ax                    ; 89 46 f8
    mov ax, word [bp-00ch]                    ; 8b 46 f4
    cmp ax, 00080h                            ; 3d 80 00
    jnbe short 05456h                         ; 77 04
    test ax, ax                               ; 85 c0
    jne short 05479h                          ; 75 23
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 4d c4
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 005eeh                               ; 68 ee 05
    push 0065ah                               ; 68 5a 06
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 76 c4
    add sp, strict byte 00008h                ; 83 c4 08
    jmp near 0573ch                           ; e9 c3 02
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    mov es, [bp-004h]                         ; 8e 46 fc
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    mov ax, word [es:bx+028h]                 ; 26 8b 47 28
    mov cx, word [es:bx+026h]                 ; 26 8b 4f 26
    mov dx, word [es:bx+02ah]                 ; 26 8b 57 2a
    mov word [bp-00ah], dx                    ; 89 56 f6
    cmp di, ax                                ; 39 c7
    jnc short 054a6h                          ; 73 0c
    cmp cx, word [bp-008h]                    ; 3b 4e f8
    jbe short 054a6h                          ; 76 07
    mov ax, word [bp-006h]                    ; 8b 46 fa
    cmp ax, dx                                ; 39 d0
    jbe short 054d4h                          ; 76 2e
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 fd c3
    push dword [bp-008h]                      ; 66 ff 76 f8
    push di                                   ; 57
    mov ax, word [bp+012h]                    ; 8b 46 12
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 005eeh                               ; 68 ee 05
    push 00682h                               ; 68 82 06
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 1b c4
    add sp, strict byte 00010h                ; 83 c4 10
    jmp near 0573ch                           ; e9 68 02
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    cmp ax, strict word 00004h                ; 3d 04 00
    jne short 054e2h                          ; 75 03
    jmp near 053d9h                           ; e9 f7 fe
    movzx bx, byte [bp-002h]                  ; 0f b6 5e fe
    imul bx, bx, strict byte 00018h           ; 6b db 18
    mov es, [bp-004h]                         ; 8e 46 fc
    add bx, si                                ; 01 f3
    cmp cx, word [es:bx+02ch]                 ; 26 3b 4f 2c
    jne short 05503h                          ; 75 0f
    mov ax, word [es:bx+030h]                 ; 26 8b 47 30
    cmp ax, word [bp-00ah]                    ; 3b 46 f6
    jne short 05503h                          ; 75 06
    cmp byte [bp-002h], 008h                  ; 80 7e fe 08
    jc short 05533h                           ; 72 30
    mov ax, di                                ; 89 f8
    xor dx, dx                                ; 31 d2
    mov bx, cx                                ; 89 cb
    xor cx, cx                                ; 31 c9
    call 08fe9h                               ; e8 db 3a
    xor bx, bx                                ; 31 db
    add ax, word [bp-008h]                    ; 03 46 f8
    adc dx, bx                                ; 11 da
    mov bx, word [bp-00ah]                    ; 8b 5e f6
    xor cx, cx                                ; 31 c9
    call 08fe9h                               ; e8 cc 3a
    xor bx, bx                                ; 31 db
    add ax, word [bp-006h]                    ; 03 46 fa
    adc dx, bx                                ; 11 da
    add ax, strict word 0ffffh                ; 05 ff ff
    mov word [bp-010h], ax                    ; 89 46 f0
    adc dx, strict byte 0ffffh                ; 83 d2 ff
    mov word [bp-00eh], dx                    ; 89 56 f2
    mov word [bp-006h], bx                    ; 89 5e fa
    mov es, [bp-004h]                         ; 8e 46 fc
    db  066h, 026h, 0c7h, 044h, 014h, 000h, 000h, 000h, 000h
    ; mov dword [es:si+014h], strict dword 000000000h ; 66 26 c7 44 14 00 00 00 00
    mov word [es:si+018h], strict word 00000h ; 26 c7 44 18 00 00
    mov ax, word [bp-010h]                    ; 8b 46 f0
    mov word [es:si], ax                      ; 26 89 04
    mov ax, word [bp-00eh]                    ; 8b 46 f2
    mov word [es:si+002h], ax                 ; 26 89 44 02
    mov ax, word [bp+010h]                    ; 8b 46 10
    mov dx, word [bp+006h]                    ; 8b 56 06
    mov word [es:si+004h], ax                 ; 26 89 44 04
    mov word [es:si+006h], dx                 ; 26 89 54 06
    mov ax, word [bp-00ch]                    ; 8b 46 f4
    mov word [es:si+00ah], ax                 ; 26 89 44 0a
    mov word [es:si+00ch], 00200h             ; 26 c7 44 0c 00 02
    mov word [es:si+00eh], di                 ; 26 89 7c 0e
    mov ax, word [bp-008h]                    ; 8b 46 f8
    mov word [es:si+010h], ax                 ; 26 89 44 10
    mov ax, word [bp-006h]                    ; 8b 46 fa
    mov word [es:si+012h], ax                 ; 26 89 44 12
    mov al, byte [bp-002h]                    ; 8a 46 fe
    mov byte [es:si+008h], al                 ; 26 88 44 08
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    movzx ax, byte [es:bx+01eh]               ; 26 0f b6 47 1e
    mov bx, ax                                ; 89 c3
    sal bx, 002h                              ; c1 e3 02
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    add ax, ax                                ; 01 c0
    add bx, ax                                ; 01 c3
    push ES                                   ; 06
    push si                                   ; 56
    call word [word bx+00014h]                ; ff 97 14 00
    mov dx, ax                                ; 89 c2
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor al, al                                ; 30 c0
    mov es, [bp-004h]                         ; 8e 46 fc
    mov bx, word [es:si+014h]                 ; 26 8b 5c 14
    or bx, ax                                 ; 09 c3
    mov word [bp+016h], bx                    ; 89 5e 16
    test dl, dl                               ; 84 d2
    je near 053d9h                            ; 0f 84 15 fe
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 df c2
    movzx ax, dl                              ; 0f b6 c2
    push ax                                   ; 50
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 005eeh                               ; 68 ee 05
    push 006c9h                               ; 68 c9 06
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 04 c3
    add sp, strict byte 0000ah                ; 83 c4 0a
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    or ah, 00ch                               ; 80 cc 0c
    jmp near 05744h                           ; e9 51 01
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 b0 c2
    push 006eah                               ; 68 ea 06
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 e3 c2
    add sp, strict byte 00004h                ; 83 c4 04
    jmp near 053d9h                           ; e9 cd fd
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    mov es, [bp-004h]                         ; 8e 46 fc
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    mov di, word [es:bx+028h]                 ; 26 8b 7f 28
    mov cx, word [es:bx+026h]                 ; 26 8b 4f 26
    mov ax, word [es:bx+02ah]                 ; 26 8b 47 2a
    mov word [bp-00ah], ax                    ; 89 46 f6
    movzx ax, byte [es:si+0019eh]             ; 26 0f b6 84 9e 01
    mov word [bp-00ch], ax                    ; 89 46 f4
    mov byte [bp+016h], 000h                  ; c6 46 16 00
    mov dx, word [bp+014h]                    ; 8b 56 14
    xor dh, dh                                ; 30 f6
    dec di                                    ; 4f
    mov ax, di                                ; 89 f8
    xor ah, ah                                ; 30 e4
    sal ax, 008h                              ; c1 e0 08
    or dx, ax                                 ; 09 c2
    mov word [bp+014h], dx                    ; 89 56 14
    shr di, 002h                              ; c1 ef 02
    and di, 000c0h                            ; 81 e7 c0 00
    mov ax, word [bp-00ah]                    ; 8b 46 f6
    xor ah, ah                                ; 30 e4
    and AL, strict byte 03fh                  ; 24 3f
    or di, ax                                 ; 09 c7
    mov ax, dx                                ; 89 d0
    xor al, dl                                ; 30 d0
    or ax, di                                 ; 09 f8
    mov word [bp+014h], ax                    ; 89 46 14
    mov dx, word [bp+012h]                    ; 8b 56 12
    xor dh, dh                                ; 30 f6
    mov ax, cx                                ; 89 c8
    sal ax, 008h                              ; c1 e0 08
    sub ax, 00100h                            ; 2d 00 01
    or dx, ax                                 ; 09 c2
    mov word [bp+012h], dx                    ; 89 56 12
    mov ax, dx                                ; 89 d0
    xor al, dl                                ; 30 d0
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    or dx, ax                                 ; 09 c2
    mov word [bp+012h], dx                    ; 89 56 12
    jmp near 053d9h                           ; e9 57 fd
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    cwd                                       ; 99
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2
    sar ax, 1                                 ; d1 f8
    imul ax, ax, strict byte 00006h           ; 6b c0 06
    mov es, [bp-004h]                         ; 8e 46 fc
    add si, ax                                ; 01 c6
    mov dx, word [es:si+001c2h]               ; 26 8b 94 c2 01
    add dx, strict byte 00007h                ; 83 c2 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and AL, strict byte 0c0h                  ; 24 c0
    cmp AL, strict byte 040h                  ; 3c 40
    jne short 056a7h                          ; 75 03
    jmp near 053d9h                           ; e9 32 fd
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    or ah, 0aah                               ; 80 cc aa
    jmp near 05744h                           ; e9 92 00
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    mov es, [bp-004h]                         ; 8e 46 fc
    add si, ax                                ; 01 c6
    mov di, word [es:si+02eh]                 ; 26 8b 7c 2e
    mov ax, word [es:si+02ch]                 ; 26 8b 44 2c
    mov word [bp-008h], ax                    ; 89 46 f8
    mov ax, word [es:si+030h]                 ; 26 8b 44 30
    mov word [bp-006h], ax                    ; 89 46 fa
    mov ax, di                                ; 89 f8
    xor dx, dx                                ; 31 d2
    mov bx, word [bp-008h]                    ; 8b 5e f8
    xor cx, cx                                ; 31 c9
    call 08fe9h                               ; e8 0d 39
    mov bx, word [bp-006h]                    ; 8b 5e fa
    xor cx, cx                                ; 31 c9
    call 08fe9h                               ; e8 05 39
    mov word [bp-010h], ax                    ; 89 46 f0
    mov word [bp-00eh], dx                    ; 89 56 f2
    mov word [bp+014h], dx                    ; 89 56 14
    mov word [bp+012h], ax                    ; 89 46 12
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    or ah, 003h                               ; 80 cc 03
    mov word [bp+016h], ax                    ; 89 46 16
    jmp near 053ddh                           ; e9 df fc
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 a5 c1
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 005eeh                               ; 68 ee 05
    push 00704h                               ; 68 04 07
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 ce c1
    add sp, strict byte 00008h                ; 83 c4 08
    jmp near 053d9h                           ; e9 b8 fc
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 82 c1
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 005eeh                               ; 68 ee 05
    push 00737h                               ; 68 37 07
    jmp near 0546eh                           ; e9 32 fd
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    or ah, 001h                               ; 80 cc 01
    mov word [bp+016h], ax                    ; 89 46 16
    mov bx, word [bp+016h]                    ; 8b 5e 16
    shr bx, 008h                              ; c1 eb 08
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 b6 be
    or byte [bp+01ch], 001h                   ; 80 4e 1c 01
    leave                                     ; c9
    retn                                      ; c3
    pop SS                                    ; 17
    pop ax                                    ; 58
    das                                       ; 2f
    pop ax                                    ; 58
    das                                       ; 2f
    pop ax                                    ; 58
    das                                       ; 2f
    pop ax                                    ; 58
    ficomp word [bp+di+072h]                  ; de 5b 72
    pop cx                                    ; 59
    das                                       ; 2f
    pop ax                                    ; 58
    js short 057c7h                           ; 78 59
    ficomp word [bp+di+02bh]                  ; de 5b 2b
    pop sp                                    ; 5c
    sub bx, word [si+02bh]                    ; 2b 5c 2b
    pop sp                                    ; 5c
    sub bx, word [si-00dh]                    ; 2b 5c f3
    pop bx                                    ; 5b
    sub bx, word [si+02bh]                    ; 2b 5c 2b
    pop sp                                    ; 5c
_int13_harddisk_ext:                         ; 0xf577e LB 0x4c8
    enter 00028h, 000h                        ; c8 28 00 00
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 91 be
    mov word [bp-014h], ax                    ; 89 46 ec
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 85 be
    mov si, 00122h                            ; be 22 01
    mov word [bp-026h], ax                    ; 89 46 da
    xor bx, bx                                ; 31 db
    mov dx, 0008eh                            ; ba 8e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 66 be
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    xor ah, ah                                ; 30 e4
    cmp ax, 00080h                            ; 3d 80 00
    jc short 057b7h                           ; 72 05
    cmp ax, 00090h                            ; 3d 90 00
    jc short 057d5h                           ; 72 1e
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 00765h                               ; 68 65 07
    push 005fdh                               ; 68 fd 05
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 1a c1
    add sp, strict byte 0000ah                ; 83 c4 0a
    jmp near 05c09h                           ; e9 34 04
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    xor ah, ah                                ; 30 e4
    mov es, [bp-026h]                         ; 8e 46 da
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    mov cl, byte [es:bx+0011fh]               ; 26 8a 8f 1f 01
    cmp cl, 010h                              ; 80 f9 10
    jc short 057fbh                           ; 72 10
    push ax                                   ; 50
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 00765h                               ; 68 65 07
    push 00628h                               ; 68 28 06
    jmp short 057cah                          ; eb cf
    mov bx, word [bp+016h]                    ; 8b 5e 16
    shr bx, 008h                              ; c1 eb 08
    sub bx, strict byte 00041h                ; 83 eb 41
    cmp bx, strict byte 0000fh                ; 83 fb 0f
    jnbe near 05c2bh                          ; 0f 87 20 04
    add bx, bx                                ; 01 db
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    jmp word [cs:bx+0575eh]                   ; 2e ff a7 5e 57
    mov word [bp+010h], 0aa55h                ; c7 46 10 55 aa
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    or ah, 030h                               ; 80 cc 30
    mov word [bp+016h], ax                    ; 89 46 16
    mov word [bp+014h], strict word 00007h    ; c7 46 14 07 00
    jmp near 05be2h                           ; e9 b3 03
    mov bx, word [bp+00ah]                    ; 8b 5e 0a
    mov es, [bp+004h]                         ; 8e 46 04
    mov di, bx                                ; 89 df
    mov [bp-010h], es                         ; 8c 46 f0
    mov ax, word [es:bx+002h]                 ; 26 8b 47 02
    mov word [bp-012h], ax                    ; 89 46 ee
    mov ax, word [es:bx+006h]                 ; 26 8b 47 06
    mov word [bp-018h], ax                    ; 89 46 e8
    mov ax, word [es:bx+004h]                 ; 26 8b 47 04
    mov word [bp-016h], ax                    ; 89 46 ea
    mov ax, word [es:bx+00ch]                 ; 26 8b 47 0c
    mov word [bp-00ah], ax                    ; 89 46 f6
    mov ax, word [es:bx+00eh]                 ; 26 8b 47 0e
    or ax, word [bp-00ah]                     ; 0b 46 f6
    je short 05870h                           ; 74 11
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 00765h                               ; 68 65 07
    push 00778h                               ; 68 78 07
    push strict byte 00007h                   ; 6a 07
    jmp short 058bah                          ; eb 4a
    mov es, [bp-010h]                         ; 8e 46 f0
    mov ax, word [es:di+008h]                 ; 26 8b 45 08
    mov word [bp-00ah], ax                    ; 89 46 f6
    mov ax, word [es:di+00ah]                 ; 26 8b 45 0a
    movzx dx, cl                              ; 0f b6 d1
    imul dx, dx, strict byte 00018h           ; 6b d2 18
    mov es, [bp-026h]                         ; 8e 46 da
    mov bx, si                                ; 89 f3
    add bx, dx                                ; 01 d3
    mov ch, byte [es:bx+01eh]                 ; 26 8a 6f 1e
    cmp ax, word [es:bx+034h]                 ; 26 3b 47 34
    jnbe short 058a0h                         ; 77 0b
    jne short 058c3h                          ; 75 2c
    mov dx, word [bp-00ah]                    ; 8b 56 f6
    cmp dx, word [es:bx+032h]                 ; 26 3b 57 32
    jc short 058c3h                           ; 72 23
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 03 c0
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 00765h                               ; 68 65 07
    push 007a1h                               ; 68 a1 07
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 2c c0
    add sp, strict byte 00008h                ; 83 c4 08
    jmp near 05c09h                           ; e9 46 03
    mov dx, word [bp+016h]                    ; 8b 56 16
    shr dx, 008h                              ; c1 ea 08
    mov word [bp-00ch], dx                    ; 89 56 f4
    cmp dx, strict byte 00044h                ; 83 fa 44
    je near 05bdeh                            ; 0f 84 0b 03
    cmp dx, strict byte 00047h                ; 83 fa 47
    je near 05bdeh                            ; 0f 84 04 03
    mov es, [bp-026h]                         ; 8e 46 da
    db  066h, 026h, 0c7h, 044h, 014h, 000h, 000h, 000h, 000h
    ; mov dword [es:si+014h], strict dword 000000000h ; 66 26 c7 44 14 00 00 00 00
    mov word [es:si+018h], strict word 00000h ; 26 c7 44 18 00 00
    mov dx, word [bp-00ah]                    ; 8b 56 f6
    mov word [es:si], dx                      ; 26 89 14
    mov word [es:si+002h], ax                 ; 26 89 44 02
    mov ax, word [bp-016h]                    ; 8b 46 ea
    mov word [es:si+004h], ax                 ; 26 89 44 04
    mov ax, word [bp-018h]                    ; 8b 46 e8
    mov word [es:si+006h], ax                 ; 26 89 44 06
    mov ax, word [bp-012h]                    ; 8b 46 ee
    mov word [es:si+00ah], ax                 ; 26 89 44 0a
    mov word [es:si+00ch], 00200h             ; 26 c7 44 0c 00 02
    mov word [es:si+012h], strict word 00000h ; 26 c7 44 12 00 00
    mov byte [es:si+008h], cl                 ; 26 88 4c 08
    mov bx, word [bp-00ch]                    ; 8b 5e f4
    add bx, bx                                ; 01 db
    movzx ax, ch                              ; 0f b6 c5
    sal ax, 002h                              ; c1 e0 02
    add bx, ax                                ; 01 c3
    push ES                                   ; 06
    push si                                   ; 56
    call word [word bx-0006ch]                ; ff 97 94 ff
    mov dx, ax                                ; 89 c2
    mov es, [bp-026h]                         ; 8e 46 da
    mov ax, word [es:si+014h]                 ; 26 8b 44 14
    mov word [bp-012h], ax                    ; 89 46 ee
    mov es, [bp-010h]                         ; 8e 46 f0
    mov word [es:di+002h], ax                 ; 26 89 45 02
    test dl, dl                               ; 84 d2
    je near 05bdeh                            ; 0f 84 97 02
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 5c bf
    movzx ax, dl                              ; 0f b6 c2
    push ax                                   ; 50
    push word [bp-00ch]                       ; ff 76 f4
    push 00765h                               ; 68 65 07
    push 006c9h                               ; 68 c9 06
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 85 bf
    add sp, strict byte 0000ah                ; 83 c4 0a
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    or ah, 00ch                               ; 80 cc 0c
    jmp near 05c11h                           ; e9 9f 02
    or ah, 0b2h                               ; 80 cc b2
    jmp near 05c11h                           ; e9 99 02
    mov bx, word [bp+00ah]                    ; 8b 5e 0a
    mov ax, word [bp+004h]                    ; 8b 46 04
    mov word [bp-006h], ax                    ; 89 46 fa
    mov word [bp-004h], ax                    ; 89 46 fc
    mov es, ax                                ; 8e c0
    mov di, bx                                ; 89 df
    mov ax, word [es:di]                      ; 26 8b 05
    mov word [bp-00eh], ax                    ; 89 46 f2
    cmp ax, strict word 0001ah                ; 3d 1a 00
    jc near 05c09h                            ; 0f 82 74 02
    jc near 05a1ch                            ; 0f 82 83 00
    movzx ax, cl                              ; 0f b6 c1
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    mov es, [bp-026h]                         ; 8e 46 da
    mov di, si                                ; 89 f7
    add di, ax                                ; 01 c7
    mov ax, word [es:di+02eh]                 ; 26 8b 45 2e
    mov word [bp-028h], ax                    ; 89 46 d8
    mov ax, word [es:di+02ch]                 ; 26 8b 45 2c
    mov word [bp-01eh], ax                    ; 89 46 e2
    mov ax, word [es:di+030h]                 ; 26 8b 45 30
    mov word [bp-024h], ax                    ; 89 46 dc
    mov ax, word [es:di+032h]                 ; 26 8b 45 32
    mov word [bp-00ah], ax                    ; 89 46 f6
    mov ax, word [es:di+034h]                 ; 26 8b 45 34
    mov dx, word [es:di+024h]                 ; 26 8b 55 24
    mov word [bp-022h], dx                    ; 89 56 de
    mov es, [bp-006h]                         ; 8e 46 fa
    mov di, bx                                ; 89 df
    db  066h, 026h, 0c7h, 005h, 01ah, 000h, 002h, 000h
    ; mov dword [es:di], strict dword 00002001ah ; 66 26 c7 05 1a 00 02 00
    mov dx, word [bp-028h]                    ; 8b 56 d8
    mov word [es:di+004h], dx                 ; 26 89 55 04
    mov word [es:di+006h], strict word 00000h ; 26 c7 45 06 00 00
    mov dx, word [bp-01eh]                    ; 8b 56 e2
    mov word [es:di+008h], dx                 ; 26 89 55 08
    mov word [es:di+00ah], strict word 00000h ; 26 c7 45 0a 00 00
    mov dx, word [bp-024h]                    ; 8b 56 dc
    mov word [es:di+00ch], dx                 ; 26 89 55 0c
    mov word [es:di+00eh], strict word 00000h ; 26 c7 45 0e 00 00
    mov dx, word [bp-022h]                    ; 8b 56 de
    mov word [es:di+018h], dx                 ; 26 89 55 18
    mov dx, word [bp-00ah]                    ; 8b 56 f6
    mov word [es:di+010h], dx                 ; 26 89 55 10
    mov word [es:di+012h], ax                 ; 26 89 45 12
    db  066h, 026h, 0c7h, 045h, 014h, 000h, 000h, 000h, 000h
    ; mov dword [es:di+014h], strict dword 000000000h ; 66 26 c7 45 14 00 00 00 00
    cmp word [bp-00eh], strict byte 0001eh    ; 83 7e f2 1e
    jc near 05b28h                            ; 0f 82 04 01
    mov es, [bp-004h]                         ; 8e 46 fc
    mov word [es:bx], strict word 0001eh      ; 26 c7 07 1e 00
    mov ax, word [bp-014h]                    ; 8b 46 ec
    mov word [es:bx+01ch], ax                 ; 26 89 47 1c
    mov word [es:bx+01ah], 00312h             ; 26 c7 47 1a 12 03
    movzx ax, cl                              ; 0f b6 c1
    mov word [bp-020h], ax                    ; 89 46 e0
    cwd                                       ; 99
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2
    sar ax, 1                                 ; d1 f8
    movzx di, al                              ; 0f b6 f8
    imul di, di, strict byte 00006h           ; 6b ff 06
    mov es, [bp-026h]                         ; 8e 46 da
    add di, si                                ; 01 f7
    mov ax, word [es:di+001c2h]               ; 26 8b 85 c2 01
    mov word [bp-01ah], ax                    ; 89 46 e6
    mov ax, word [es:di+001c4h]               ; 26 8b 85 c4 01
    mov word [bp-01ch], ax                    ; 89 46 e4
    mov ch, byte [es:di+001c1h]               ; 26 8a ad c1 01
    imul di, word [bp-020h], strict byte 00018h ; 6b 7e e0 18
    add di, si                                ; 01 f7
    mov ah, byte [es:di+022h]                 ; 26 8a 65 22
    mov al, byte [es:di+023h]                 ; 26 8a 45 23
    test al, al                               ; 84 c0
    jne short 05a7ah                          ; 75 04
    xor dx, dx                                ; 31 d2
    jmp short 05a7dh                          ; eb 03
    mov dx, strict word 00008h                ; ba 08 00
    or dl, 010h                               ; 80 ca 10
    mov word [bp-008h], dx                    ; 89 56 f8
    cmp ah, 001h                              ; 80 fc 01
    db  00fh, 094h, 0c4h
    ; sete ah                                   ; 0f 94 c4
    movzx dx, ah                              ; 0f b6 d4
    or word [bp-008h], dx                     ; 09 56 f8
    cmp AL, strict byte 001h                  ; 3c 01
    db  00fh, 094h, 0c4h
    ; sete ah                                   ; 0f 94 c4
    movzx dx, ah                              ; 0f b6 d4
    or word [bp-008h], dx                     ; 09 56 f8
    cmp AL, strict byte 003h                  ; 3c 03
    jne short 05aa3h                          ; 75 05
    mov ax, strict word 00003h                ; b8 03 00
    jmp short 05aa5h                          ; eb 02
    xor ax, ax                                ; 31 c0
    or word [bp-008h], ax                     ; 09 46 f8
    mov ax, word [bp-01ah]                    ; 8b 46 e6
    mov es, [bp-026h]                         ; 8e 46 da
    mov word [es:si+001f0h], ax               ; 26 89 84 f0 01
    mov ax, word [bp-01ch]                    ; 8b 46 e4
    mov word [es:si+001f2h], ax               ; 26 89 84 f2 01
    movzx ax, cl                              ; 0f b6 c1
    cwd                                       ; 99
    mov di, strict word 00002h                ; bf 02 00
    idiv di                                   ; f7 ff
    or dl, 00eh                               ; 80 ca 0e
    sal dx, 004h                              ; c1 e2 04
    mov byte [es:si+001f4h], dl               ; 26 88 94 f4 01
    mov byte [es:si+001f5h], 0cbh             ; 26 c6 84 f5 01 cb
    mov byte [es:si+001f6h], ch               ; 26 88 ac f6 01
    mov word [es:si+001f7h], strict word 00001h ; 26 c7 84 f7 01 01 00
    mov byte [es:si+001f9h], 000h             ; 26 c6 84 f9 01 00
    mov ax, word [bp-008h]                    ; 8b 46 f8
    mov word [es:si+001fah], ax               ; 26 89 84 fa 01
    mov word [es:si+001fch], strict word 00000h ; 26 c7 84 fc 01 00 00
    mov byte [es:si+001feh], 011h             ; 26 c6 84 fe 01 11
    xor ch, ch                                ; 30 ed
    mov byte [bp-002h], ch                    ; 88 6e fe
    jmp short 05b09h                          ; eb 06
    cmp byte [bp-002h], 00fh                  ; 80 7e fe 0f
    jnc short 05b1eh                          ; 73 15
    movzx dx, byte [bp-002h]                  ; 0f b6 56 fe
    add dx, 00312h                            ; 81 c2 12 03
    mov ax, word [bp-014h]                    ; 8b 46 ec
    call 01600h                               ; e8 e9 ba
    add ch, al                                ; 00 c5
    inc byte [bp-002h]                        ; fe 46 fe
    jmp short 05b03h                          ; eb e5
    neg ch                                    ; f6 dd
    mov es, [bp-026h]                         ; 8e 46 da
    mov byte [es:si+001ffh], ch               ; 26 88 ac ff 01
    cmp word [bp-00eh], strict byte 00042h    ; 83 7e f2 42
    jc near 05bdeh                            ; 0f 82 ae 00
    movzx ax, cl                              ; 0f b6 c1
    cwd                                       ; 99
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2
    sar ax, 1                                 ; d1 f8
    xor ah, ah                                ; 30 e4
    imul ax, ax, strict byte 00006h           ; 6b c0 06
    mov es, [bp-026h]                         ; 8e 46 da
    add si, ax                                ; 01 c6
    mov al, byte [es:si+001c0h]               ; 26 8a 84 c0 01
    mov dx, word [es:si+001c2h]               ; 26 8b 94 c2 01
    mov es, [bp-004h]                         ; 8e 46 fc
    mov word [es:bx], strict word 00042h      ; 26 c7 07 42 00
    db  066h, 026h, 0c7h, 047h, 01eh, 0ddh, 0beh, 024h, 000h
    ; mov dword [es:bx+01eh], strict dword 00024beddh ; 66 26 c7 47 1e dd be 24 00
    mov word [es:bx+022h], strict word 00000h ; 26 c7 47 22 00 00
    test al, al                               ; 84 c0
    jne short 05b70h                          ; 75 09
    db  066h, 026h, 0c7h, 047h, 024h, 049h, 053h, 041h, 020h
    ; mov dword [es:bx+024h], strict dword 020415349h ; 66 26 c7 47 24 49 53 41 20
    mov es, [bp-004h]                         ; 8e 46 fc
    db  066h, 026h, 0c7h, 047h, 028h, 041h, 054h, 041h, 020h
    ; mov dword [es:bx+028h], strict dword 020415441h ; 66 26 c7 47 28 41 54 41 20
    db  066h, 026h, 0c7h, 047h, 02ch, 020h, 020h, 020h, 020h
    ; mov dword [es:bx+02ch], strict dword 020202020h ; 66 26 c7 47 2c 20 20 20 20
    test al, al                               ; 84 c0
    jne short 05b9ch                          ; 75 13
    mov word [es:bx+030h], dx                 ; 26 89 57 30
    db  066h, 026h, 0c7h, 047h, 032h, 000h, 000h, 000h, 000h
    ; mov dword [es:bx+032h], strict dword 000000000h ; 66 26 c7 47 32 00 00 00 00
    mov word [es:bx+036h], strict word 00000h ; 26 c7 47 36 00 00
    mov al, cl                                ; 88 c8
    and AL, strict byte 001h                  ; 24 01
    xor ah, ah                                ; 30 e4
    mov es, [bp-004h]                         ; 8e 46 fc
    mov word [es:bx+038h], ax                 ; 26 89 47 38
    db  066h, 026h, 0c7h, 047h, 03ah, 000h, 000h, 000h, 000h
    ; mov dword [es:bx+03ah], strict dword 000000000h ; 66 26 c7 47 3a 00 00 00 00
    mov word [es:bx+03eh], strict word 00000h ; 26 c7 47 3e 00 00
    xor cl, cl                                ; 30 c9
    mov CH, strict byte 01eh                  ; b5 1e
    jmp short 05bc3h                          ; eb 05
    cmp ch, 040h                              ; 80 fd 40
    jnc short 05bd5h                          ; 73 12
    movzx dx, ch                              ; 0f b6 d5
    add dx, word [bp+00ah]                    ; 03 56 0a
    mov ax, word [bp+004h]                    ; 8b 46 04
    call 01600h                               ; e8 31 ba
    add cl, al                                ; 00 c1
    db  0feh, 0c5h
    ; inc ch                                    ; fe c5
    jmp short 05bbeh                          ; eb e9
    neg cl                                    ; f6 d9
    mov es, [bp-004h]                         ; 8e 46 fc
    mov byte [es:bx+041h], cl                 ; 26 88 4f 41
    mov byte [bp+017h], 000h                  ; c6 46 17 00
    xor bx, bx                                ; 31 db
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 21 ba
    and byte [bp+01ch], 0feh                  ; 80 66 1c fe
    leave                                     ; c9
    retn                                      ; c3
    cmp ax, strict word 00006h                ; 3d 06 00
    je short 05bdeh                           ; 74 e6
    cmp ax, strict word 00001h                ; 3d 01 00
    jc short 05c09h                           ; 72 0c
    jbe short 05bdeh                          ; 76 df
    cmp ax, strict word 00003h                ; 3d 03 00
    jc short 05c09h                           ; 72 05
    cmp ax, strict word 00004h                ; 3d 04 00
    jbe short 05bdeh                          ; 76 d5
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    or ah, 001h                               ; 80 cc 01
    mov word [bp+016h], ax                    ; 89 46 16
    mov bx, word [bp+016h]                    ; 8b 5e 16
    shr bx, 008h                              ; c1 eb 08
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 e9 b9
    or byte [bp+01ch], 001h                   ; 80 4e 1c 01
    leave                                     ; c9
    retn                                      ; c3
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 78 bc
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 00765h                               ; 68 65 07
    push 00737h                               ; 68 37 07
    jmp near 058b8h                           ; e9 72 fc
_int14_function:                             ; 0xf5c46 LB 0x154
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sti                                       ; fb
    mov dx, word [bp+010h]                    ; 8b 56 10
    add dx, dx                                ; 01 d2
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 c6 b9
    mov si, ax                                ; 89 c6
    mov bx, ax                                ; 89 c3
    mov dx, word [bp+010h]                    ; 8b 56 10
    add dx, strict byte 0007ch                ; 83 c2 7c
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 9a b9
    mov cl, al                                ; 88 c1
    cmp word [bp+010h], strict byte 00004h    ; 83 7e 10 04
    jnc near 05d93h                           ; 0f 83 23 01
    test si, si                               ; 85 f6
    jbe near 05d93h                           ; 0f 86 1d 01
    mov al, byte [bp+015h]                    ; 8a 46 15
    cmp AL, strict byte 001h                  ; 3c 01
    jc short 05c8eh                           ; 72 11
    jbe short 05ce2h                          ; 76 63
    cmp AL, strict byte 003h                  ; 3c 03
    je near 05d7bh                            ; 0f 84 f6 00
    cmp AL, strict byte 002h                  ; 3c 02
    je near 05d30h                            ; 0f 84 a5 00
    jmp near 05d8ch                           ; e9 fe 00
    test al, al                               ; 84 c0
    jne near 05d8ch                           ; 0f 85 f8 00
    lea dx, [bx+003h]                         ; 8d 57 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    or AL, strict byte 080h                   ; 0c 80
    out DX, AL                                ; ee
    mov al, byte [bp+014h]                    ; 8a 46 14
    and AL, strict byte 0e0h                  ; 24 e0
    movzx cx, al                              ; 0f b6 c8
    sar cx, 005h                              ; c1 f9 05
    mov ax, 00600h                            ; b8 00 06
    sar ax, CL                                ; d3 f8
    mov dx, bx                                ; 89 da
    out DX, AL                                ; ee
    shr ax, 008h                              ; c1 e8 08
    lea dx, [bx+001h]                         ; 8d 57 01
    out DX, AL                                ; ee
    mov al, byte [bp+014h]                    ; 8a 46 14
    and AL, strict byte 01fh                  ; 24 1f
    lea dx, [bx+003h]                         ; 8d 57 03
    out DX, AL                                ; ee
    lea dx, [bx+005h]                         ; 8d 57 05
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov byte [bp+015h], al                    ; 88 46 15
    lea dx, [bx+006h]                         ; 8d 57 06
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov byte [bp+014h], al                    ; 88 46 14
    jmp near 05d6ch                           ; e9 97 00
    mov AL, strict byte 017h                  ; b0 17
    mov dx, bx                                ; 89 da
    out DX, AL                                ; ee
    lea dx, [bx+001h]                         ; 8d 57 01
    mov AL, strict byte 004h                  ; b0 04
    out DX, AL                                ; ee
    jmp short 05cb7h                          ; eb d5
    mov dx, strict word 0006ch                ; ba 6c 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 31 b9
    mov si, ax                                ; 89 c6
    lea dx, [bx+005h]                         ; 8d 57 05
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and ax, strict word 00060h                ; 25 60 00
    cmp ax, strict word 00060h                ; 3d 60 00
    je short 05d12h                           ; 74 17
    test cl, cl                               ; 84 c9
    je short 05d12h                           ; 74 13
    mov dx, strict word 0006ch                ; ba 6c 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 14 b9
    cmp ax, si                                ; 39 f0
    je short 05cedh                           ; 74 e1
    mov si, ax                                ; 89 c6
    db  0feh, 0c9h
    ; dec cl                                    ; fe c9
    jmp short 05cedh                          ; eb db
    test cl, cl                               ; 84 c9
    je short 05d1ch                           ; 74 06
    mov al, byte [bp+014h]                    ; 8a 46 14
    mov dx, bx                                ; 89 da
    out DX, AL                                ; ee
    lea dx, [bx+005h]                         ; 8d 57 05
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov byte [bp+015h], al                    ; 88 46 15
    test cl, cl                               ; 84 c9
    jne short 05d6ch                          ; 75 43
    or AL, strict byte 080h                   ; 0c 80
    mov byte [bp+015h], al                    ; 88 46 15
    jmp short 05d6ch                          ; eb 3c
    mov dx, strict word 0006ch                ; ba 6c 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 e3 b8
    mov si, ax                                ; 89 c6
    lea dx, [bx+005h]                         ; 8d 57 05
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 05d5ch                          ; 75 17
    test cl, cl                               ; 84 c9
    je short 05d5ch                           ; 74 13
    mov dx, strict word 0006ch                ; ba 6c 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 ca b8
    cmp ax, si                                ; 39 f0
    je short 05d3bh                           ; 74 e5
    mov si, ax                                ; 89 c6
    db  0feh, 0c9h
    ; dec cl                                    ; fe c9
    jmp short 05d3bh                          ; eb df
    test cl, cl                               ; 84 c9
    je short 05d73h                           ; 74 13
    mov byte [bp+015h], 000h                  ; c6 46 15 00
    mov dx, bx                                ; 89 da
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov byte [bp+014h], al                    ; 88 46 14
    and byte [bp+01eh], 0feh                  ; 80 66 1e fe
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn                                      ; c3
    lea dx, [bx+005h]                         ; 8d 57 05
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    jmp short 05d2bh                          ; eb b0
    lea dx, [si+005h]                         ; 8d 54 05
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov byte [bp+015h], al                    ; 88 46 15
    lea dx, [si+006h]                         ; 8d 54 06
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    jmp short 05d69h                          ; eb dd
    or byte [bp+01eh], 001h                   ; 80 4e 1e 01
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn                                      ; c3
    or byte [bp+01eh], 001h                   ; 80 4e 1e 01
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn                                      ; c3
set_enable_a20_:                             ; 0xf5d9a LB 0x29
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, ax                                ; 89 c3
    mov dx, 00092h                            ; ba 92 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov cl, al                                ; 88 c1
    test bx, bx                               ; 85 db
    je short 05db3h                           ; 74 05
    or AL, strict byte 002h                   ; 0c 02
    out DX, AL                                ; ee
    jmp short 05db6h                          ; eb 03
    and AL, strict byte 0fdh                  ; 24 fd
    out DX, AL                                ; ee
    test cl, 002h                             ; f6 c1 02
    db  00fh, 095h, 0c0h
    ; setne al                                  ; 0f 95 c0
    xor ah, ah                                ; 30 e4
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
set_e820_range_:                             ; 0xf5dc3 LB 0x8c
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov es, ax                                ; 8e c0
    mov si, dx                                ; 89 d6
    mov word [es:si], bx                      ; 26 89 1c
    mov word [es:si+002h], cx                 ; 26 89 4c 02
    movzx ax, byte [bp+00ah]                  ; 0f b6 46 0a
    mov word [es:si+004h], ax                 ; 26 89 44 04
    mov word [es:si+006h], strict word 00000h ; 26 c7 44 06 00 00
    sub word [bp+006h], bx                    ; 29 5e 06
    sbb word [bp+008h], cx                    ; 19 4e 08
    mov al, byte [bp+00ah]                    ; 8a 46 0a
    sub byte [bp+00ch], al                    ; 28 46 0c
    mov ax, word [bp+006h]                    ; 8b 46 06
    mov word [es:si+008h], ax                 ; 26 89 44 08
    mov ax, word [bp+008h]                    ; 8b 46 08
    mov word [es:si+00ah], ax                 ; 26 89 44 0a
    movzx ax, byte [bp+00ch]                  ; 0f b6 46 0c
    mov word [es:si+00ch], ax                 ; 26 89 44 0c
    mov word [es:si+00eh], strict word 00000h ; 26 c7 44 0e 00 00
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    mov word [es:si+010h], ax                 ; 26 89 44 10
    mov word [es:si+012h], strict word 00000h ; 26 c7 44 12 00 00
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn 0000ah                               ; c2 0a 00
    in AL, DX                                 ; ec
    jmp near 01ff6h                           ; e9 d8 c1
    sar byte [bx-06f6fh], 089h                ; c0 bf 91 90 89
    mov byte [bx+05283h], al                  ; 88 87 83 52
    dec di                                    ; 4f
    inc cx                                    ; 41
    and AL, strict byte 000h                  ; 24 00
    jmp far 0a05eh:08d62h                     ; ea 62 8d 5e a0
    pop si                                    ; 5e
    xor bl, byte [bx+038h]                    ; 32 5f 38
    pop di                                    ; 5f
    cmp ax, 0425fh                            ; 3d 5f 42
    pop di                                    ; 5f
    in AL, strict byte 05fh                   ; e4 5f
    and word [bx+di-059h], 02e61h             ; 81 61 a7 61 2e
    pop di                                    ; 5f
    db  02eh, 05fh
    ; cs pop di                                 ; 2e 5f
    je short 05ea7h                           ; 74 62
    pushfw                                    ; 9c
    bound bp, [bx-0419eh]                     ; 62 af 62 be
    bound si, [bp+si]                         ; 62 32
    pop di                                    ; 5f
    db  0c5h
    db  062h
_int15_function:                             ; 0xf5e4f LB 0x4c9
    push si                                   ; 56
    push di                                   ; 57
    enter 00004h, 000h                        ; c8 04 00 00
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    cmp ax, 000ech                            ; 3d ec 00
    jnbe near 062eah                          ; 0f 87 88 04
    push CS                                   ; 0e
    pop ES                                    ; 07
    mov cx, strict word 00012h                ; b9 12 00
    mov di, 05e1ah                            ; bf 1a 5e
    repne scasb                               ; f2 ae
    sal cx, 1                                 ; d1 e1
    mov di, cx                                ; 89 cf
    mov si, word [cs:di+05e2bh]               ; 2e 8b b5 2b 5e
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    mov cx, word [bp+01ch]                    ; 8b 4e 1c
    and cl, 0feh                              ; 80 e1 fe
    mov bx, word [bp+01ch]                    ; 8b 5e 1c
    or bl, 001h                               ; 80 cb 01
    mov dx, ax                                ; 89 c2
    or dh, 086h                               ; 80 ce 86
    jmp si                                    ; ff e6
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    cmp ax, 000c0h                            ; 3d c0 00
    jne near 062eah                           ; 0f 85 51 04
    or byte [bp+01ch], 001h                   ; 80 4e 1c 01
    jmp near 06293h                           ; e9 f3 03
    mov dx, ax                                ; 89 c2
    cmp ax, strict word 00001h                ; 3d 01 00
    jc short 05eb5h                           ; 72 0e
    jbe short 05ec9h                          ; 76 20
    cmp ax, strict word 00003h                ; 3d 03 00
    je short 05ef6h                           ; 74 48
    cmp ax, strict word 00002h                ; 3d 02 00
    je short 05ed9h                           ; 74 26
    jmp short 05f03h                          ; eb 4e
    test ax, ax                               ; 85 c0
    jne short 05f03h                          ; 75 4a
    xor ax, ax                                ; 31 c0
    call 05d9ah                               ; e8 dc fe
    and byte [bp+01ch], 0feh                  ; 80 66 1c fe
    mov byte [bp+017h], 000h                  ; c6 46 17 00
    jmp near 05f2eh                           ; e9 65 00
    mov ax, strict word 00001h                ; b8 01 00
    call 05d9ah                               ; e8 cb fe
    and byte [bp+01ch], 0feh                  ; 80 66 1c fe
    mov byte [bp+017h], dh                    ; 88 76 17
    jmp near 05f2eh                           ; e9 55 00
    mov dx, 00092h                            ; ba 92 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    shr ax, 1                                 ; d1 e8
    and ax, strict word 00001h                ; 25 01 00
    mov dx, word [bp+016h]                    ; 8b 56 16
    mov dl, al                                ; 88 c2
    mov word [bp+016h], dx                    ; 89 56 16
    and byte [bp+01ch], 0feh                  ; 80 66 1c fe
    mov byte [bp+017h], ah                    ; 88 66 17
    jmp near 05f2eh                           ; e9 38 00
    and byte [bp+01ch], 0feh                  ; 80 66 1c fe
    mov byte [bp+017h], ah                    ; 88 66 17
    mov word [bp+010h], ax                    ; 89 46 10
    jmp near 05f2eh                           ; e9 2b 00
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 a0 b9
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    push 007c6h                               ; 68 c6 07
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 cd b9
    add sp, strict byte 00006h                ; 83 c4 06
    or byte [bp+01ch], 001h                   ; 80 4e 1c 01
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    or ah, 086h                               ; 80 cc 86
    mov word [bp+016h], ax                    ; 89 46 16
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
    mov word [bp+01ch], bx                    ; 89 5e 1c
    jmp near 05fdeh                           ; e9 a6 00
    mov word [bp+01ch], bx                    ; 89 5e 1c
    jmp short 05f2eh                          ; eb f1
    mov word [bp+01ch], cx                    ; 89 4e 1c
    jmp short 05f2bh                          ; eb e9
    test byte [bp+016h], 0ffh                 ; f6 46 16 ff
    je short 05fb4h                           ; 74 6c
    mov dx, 000a0h                            ; ba a0 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 af b6
    test AL, strict byte 001h                 ; a8 01
    jne near 0628ah                           ; 0f 85 33 03
    mov bx, strict word 00001h                ; bb 01 00
    mov dx, 000a0h                            ; ba a0 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 ab b6
    mov bx, word [bp+018h]                    ; 8b 5e 18
    mov dx, 00098h                            ; ba 98 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0162ah                               ; e8 bb b6
    mov bx, word [bp+010h]                    ; 8b 5e 10
    mov dx, 0009ah                            ; ba 9a 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0162ah                               ; e8 af b6
    mov bx, word [bp+012h]                    ; 8b 5e 12
    mov dx, 0009ch                            ; ba 9c 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0162ah                               ; e8 a3 b6
    mov bx, word [bp+014h]                    ; 8b 5e 14
    mov dx, 0009eh                            ; ba 9e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0162ah                               ; e8 97 b6
    and byte [bp+01ch], 0feh                  ; 80 66 1c fe
    mov dx, 000a1h                            ; ba a1 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and AL, strict byte 0feh                  ; 24 fe
    out DX, AL                                ; ee
    mov ax, strict word 0000bh                ; b8 0b 00
    call 0165ch                               ; e8 b6 b6
    or AL, strict byte 040h                   ; 0c 40
    movzx dx, al                              ; 0f b6 d0
    mov ax, strict word 0000bh                ; b8 0b 00
    call 0166dh                               ; e8 bc b6
    jmp near 05f2eh                           ; e9 7a ff
    cmp ax, strict word 00001h                ; 3d 01 00
    jne short 05fd2h                          ; 75 19
    xor bx, bx                                ; 31 db
    mov dx, 000a0h                            ; ba a0 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 4a b6
    and byte [bp+01ch], 0feh                  ; 80 66 1c fe
    mov ax, strict word 0000bh                ; b8 0b 00
    call 0165ch                               ; e8 8e b6
    and AL, strict byte 0bfh                  ; 24 bf
    jmp short 05fa8h                          ; eb d6
    mov word [bp+01ch], bx                    ; 89 5e 1c
    mov ax, dx                                ; 89 d0
    xor ah, dh                                ; 30 f4
    xor dl, dl                                ; 30 d2
    dec ax                                    ; 48
    or dx, ax                                 ; 09 c2
    mov word [bp+016h], dx                    ; 89 56 16
    jmp near 05f2eh                           ; e9 4a ff
    cli                                       ; fa
    mov ax, strict word 00001h                ; b8 01 00
    call 05d9ah                               ; e8 af fd
    mov di, ax                                ; 89 c7
    mov ax, word [bp+018h]                    ; 8b 46 18
    sal ax, 004h                              ; c1 e0 04
    mov cx, word [bp+00ah]                    ; 8b 4e 0a
    add cx, ax                                ; 01 c1
    mov dx, word [bp+018h]                    ; 8b 56 18
    shr dx, 00ch                              ; c1 ea 0c
    mov byte [bp-002h], dl                    ; 88 56 fe
    cmp cx, ax                                ; 39 c1
    jnc short 0600ah                          ; 73 05
    db  0feh, 0c2h
    ; inc dl                                    ; fe c2
    mov byte [bp-002h], dl                    ; 88 56 fe
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 00008h                ; 83 c2 08
    mov ax, word [bp+018h]                    ; 8b 46 18
    mov bx, strict word 0002fh                ; bb 2f 00
    call 0162ah                               ; e8 11 b6
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 0000ah                ; 83 c2 0a
    mov ax, word [bp+018h]                    ; 8b 46 18
    mov bx, cx                                ; 89 cb
    call 0162ah                               ; e8 03 b6
    movzx bx, byte [bp-002h]                  ; 0f b6 5e fe
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 0000ch                ; 83 c2 0c
    mov ax, word [bp+018h]                    ; 8b 46 18
    call 0160eh                               ; e8 d7 b5
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 0000dh                ; 83 c2 0d
    mov ax, word [bp+018h]                    ; 8b 46 18
    mov bx, 00093h                            ; bb 93 00
    call 0160eh                               ; e8 c8 b5
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 0000eh                ; 83 c2 0e
    mov ax, word [bp+018h]                    ; 8b 46 18
    xor bx, bx                                ; 31 db
    call 0162ah                               ; e8 d6 b5
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 00020h                ; 83 c2 20
    mov ax, word [bp+018h]                    ; 8b 46 18
    mov bx, strict word 0ffffh                ; bb ff ff
    call 0162ah                               ; e8 c7 b5
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 00022h                ; 83 c2 22
    mov ax, word [bp+018h]                    ; 8b 46 18
    xor bx, bx                                ; 31 db
    call 0162ah                               ; e8 b9 b5
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 00024h                ; 83 c2 24
    mov ax, word [bp+018h]                    ; 8b 46 18
    mov bx, strict word 0000fh                ; bb 0f 00
    call 0160eh                               ; e8 8e b5
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 00025h                ; 83 c2 25
    mov ax, word [bp+018h]                    ; 8b 46 18
    mov bx, 0009bh                            ; bb 9b 00
    call 0160eh                               ; e8 7f b5
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 00026h                ; 83 c2 26
    mov ax, word [bp+018h]                    ; 8b 46 18
    xor bx, bx                                ; 31 db
    call 0162ah                               ; e8 8d b5
    mov ax, ss                                ; 8c d0
    mov cx, ax                                ; 89 c1
    sal cx, 004h                              ; c1 e1 04
    shr ax, 00ch                              ; c1 e8 0c
    mov word [bp-004h], ax                    ; 89 46 fc
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 00028h                ; 83 c2 28
    mov ax, word [bp+018h]                    ; 8b 46 18
    mov bx, strict word 0ffffh                ; bb ff ff
    call 0162ah                               ; e8 71 b5
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 0002ah                ; 83 c2 2a
    mov ax, word [bp+018h]                    ; 8b 46 18
    mov bx, cx                                ; 89 cb
    call 0162ah                               ; e8 63 b5
    movzx bx, byte [bp-004h]                  ; 0f b6 5e fc
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 0002ch                ; 83 c2 2c
    mov ax, word [bp+018h]                    ; 8b 46 18
    call 0160eh                               ; e8 37 b5
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 0002dh                ; 83 c2 2d
    mov ax, word [bp+018h]                    ; 8b 46 18
    mov bx, 00093h                            ; bb 93 00
    call 0160eh                               ; e8 28 b5
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 0002eh                ; 83 c2 2e
    mov ax, word [bp+018h]                    ; 8b 46 18
    xor bx, bx                                ; 31 db
    call 0162ah                               ; e8 36 b5
    mov si, word [bp+00ah]                    ; 8b 76 0a
    mov es, [bp+018h]                         ; 8e 46 18
    mov cx, word [bp+014h]                    ; 8b 4e 14
    push DS                                   ; 1e
    push eax                                  ; 66 50
    db  066h, 033h, 0c0h
    ; xor eax, eax                              ; 66 33 c0
    mov ds, ax                                ; 8e d8
    mov word [00467h], sp                     ; 89 26 67 04
    mov [00469h], ss                          ; 8c 16 69 04
    call 06110h                               ; e8 00 00
    pop di                                    ; 5f
    add di, strict byte 0001bh                ; 83 c7 1b
    push strict byte 00020h                   ; 6a 20
    push di                                   ; 57
    lgdt [es:si+008h]                         ; 26 0f 01 54 08
    lidt [cs:0efe1h]                          ; 2e 0f 01 1e e1 ef
    mov eax, cr0                              ; 0f 20 c0
    or AL, strict byte 001h                   ; 0c 01
    mov cr0, eax                              ; 0f 22 c0
    retf                                      ; cb
    mov ax, strict word 00028h                ; b8 28 00
    mov ss, ax                                ; 8e d0
    mov ax, strict word 00010h                ; b8 10 00
    mov ds, ax                                ; 8e d8
    mov ax, strict word 00018h                ; b8 18 00
    mov es, ax                                ; 8e c0
    db  033h, 0f6h
    ; xor si, si                                ; 33 f6
    db  033h, 0ffh
    ; xor di, di                                ; 33 ff
    cld                                       ; fc
    rep movsw                                 ; f3 a5
    call 06144h                               ; e8 00 00
    pop ax                                    ; 58
    push 0f000h                               ; 68 00 f0
    add ax, strict byte 00018h                ; 83 c0 18
    push ax                                   ; 50
    mov ax, strict word 00028h                ; b8 28 00
    mov ds, ax                                ; 8e d8
    mov es, ax                                ; 8e c0
    mov eax, cr0                              ; 0f 20 c0
    and AL, strict byte 0feh                  ; 24 fe
    mov cr0, eax                              ; 0f 22 c0
    retf                                      ; cb
    lidt [cs:0efe7h]                          ; 2e 0f 01 1e e7 ef
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    mov ds, ax                                ; 8e d8
    mov es, ax                                ; 8e c0
    lss sp, [00467h]                          ; 0f b2 26 67 04
    pop eax                                   ; 66 58
    pop DS                                    ; 1f
    mov ax, di                                ; 89 f8
    call 05d9ah                               ; e8 25 fc
    sti                                       ; fb
    mov byte [bp+017h], 000h                  ; c6 46 17 00
    and byte [bp+01ch], 0feh                  ; 80 66 1c fe
    jmp near 05f2eh                           ; e9 ad fd
    mov ax, strict word 00031h                ; b8 31 00
    call 0165ch                               ; e8 d5 b4
    xor ah, ah                                ; 30 e4
    mov dx, ax                                ; 89 c2
    sal dx, 008h                              ; c1 e2 08
    mov ax, strict word 00030h                ; b8 30 00
    call 0165ch                               ; e8 c8 b4
    xor ah, ah                                ; 30 e4
    or dx, ax                                 ; 09 c2
    mov word [bp+016h], dx                    ; 89 56 16
    cmp dx, strict byte 0ffc0h                ; 83 fa c0
    jbe short 0617ah                          ; 76 da
    mov word [bp+016h], strict word 0ffc0h    ; c7 46 16 c0 ff
    jmp short 0617ah                          ; eb d3
    cli                                       ; fa
    mov ax, strict word 00001h                ; b8 01 00
    call 05d9ah                               ; e8 ec fb
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 00038h                ; 83 c2 38
    mov ax, word [bp+018h]                    ; 8b 46 18
    mov bx, strict word 0ffffh                ; bb ff ff
    call 0162ah                               ; e8 6d b4
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 0003ah                ; 83 c2 3a
    mov ax, word [bp+018h]                    ; 8b 46 18
    xor bx, bx                                ; 31 db
    call 0162ah                               ; e8 5f b4
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 0003ch                ; 83 c2 3c
    mov ax, word [bp+018h]                    ; 8b 46 18
    mov bx, strict word 0000fh                ; bb 0f 00
    call 0160eh                               ; e8 34 b4
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 0003dh                ; 83 c2 3d
    mov ax, word [bp+018h]                    ; 8b 46 18
    mov bx, 0009bh                            ; bb 9b 00
    call 0160eh                               ; e8 25 b4
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 0003eh                ; 83 c2 3e
    mov ax, word [bp+018h]                    ; 8b 46 18
    xor bx, bx                                ; 31 db
    call 0162ah                               ; e8 33 b4
    mov AL, strict byte 011h                  ; b0 11
    mov dx, strict word 00020h                ; ba 20 00
    out DX, AL                                ; ee
    mov dx, 000a0h                            ; ba a0 00
    out DX, AL                                ; ee
    mov ax, word [bp+010h]                    ; 8b 46 10
    shr ax, 008h                              ; c1 e8 08
    mov dx, strict word 00021h                ; ba 21 00
    out DX, AL                                ; ee
    mov ax, word [bp+010h]                    ; 8b 46 10
    mov dx, 000a1h                            ; ba a1 00
    out DX, AL                                ; ee
    mov AL, strict byte 004h                  ; b0 04
    mov dx, strict word 00021h                ; ba 21 00
    out DX, AL                                ; ee
    mov AL, strict byte 002h                  ; b0 02
    mov dx, 000a1h                            ; ba a1 00
    out DX, AL                                ; ee
    mov AL, strict byte 001h                  ; b0 01
    mov dx, strict word 00021h                ; ba 21 00
    out DX, AL                                ; ee
    mov dx, 000a1h                            ; ba a1 00
    out DX, AL                                ; ee
    mov AL, strict byte 0ffh                  ; b0 ff
    mov dx, strict word 00021h                ; ba 21 00
    out DX, AL                                ; ee
    mov dx, 000a1h                            ; ba a1 00
    out DX, AL                                ; ee
    mov si, word [bp+00ah]                    ; 8b 76 0a
    call 06238h                               ; e8 00 00
    pop di                                    ; 5f
    add di, strict byte 00018h                ; 83 c7 18
    push strict byte 00038h                   ; 6a 38
    push di                                   ; 57
    lgdt [es:si+008h]                         ; 26 0f 01 54 08
    lidt [es:si+010h]                         ; 26 0f 01 5c 10
    mov ax, strict word 00001h                ; b8 01 00
    lmsw ax                                   ; 0f 01 f0
    retf                                      ; cb
    mov ax, strict word 00028h                ; b8 28 00
    mov ss, ax                                ; 8e d0
    mov ax, strict word 00018h                ; b8 18 00
    mov ds, ax                                ; 8e d8
    mov ax, strict word 00020h                ; b8 20 00
    mov es, ax                                ; 8e c0
    lea ax, [bp+008h]                         ; 8d 46 08
    db  08bh, 0e0h
    ; mov sp, ax                                ; 8b e0
    popaw                                     ; 61
    add sp, strict byte 00006h                ; 83 c4 06
    pop cx                                    ; 59
    pop ax                                    ; 58
    pop ax                                    ; 58
    mov ax, strict word 00030h                ; b8 30 00
    push ax                                   ; 50
    push cx                                   ; 51
    retf                                      ; cb
    jmp near 05f2eh                           ; e9 ba fc
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 2f b6
    push 00806h                               ; 68 06 08
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 62 b6
    add sp, strict byte 00004h                ; 83 c4 04
    or byte [bp+01ch], 001h                   ; 80 4e 1c 01
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    or ah, 086h                               ; 80 cc 86
    mov word [bp+016h], ax                    ; 89 46 16
    jmp near 05f2eh                           ; e9 92 fc
    mov word [bp+01ch], cx                    ; 89 4e 1c
    mov word [bp+016h], ax                    ; 89 46 16
    mov word [bp+010h], 0e6f5h                ; c7 46 10 f5 e6
    mov word [bp+018h], 0f000h                ; c7 46 18 00 f0
    jmp near 05f2eh                           ; e9 7f fc
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 64 b3
    mov word [bp+018h], ax                    ; 89 46 18
    jmp near 0617ah                           ; e9 bc fe
    push 00835h                               ; 68 35 08
    push strict byte 00008h                   ; 6a 08
    jmp short 06284h                          ; eb bf
    test byte [bp+016h], 0ffh                 ; f6 46 16 ff
    jne short 062eah                          ; 75 1f
    mov word [bp+016h], ax                    ; 89 46 16
    mov ax, word [bp+010h]                    ; 8b 46 10
    xor ah, ah                                ; 30 e4
    cmp ax, strict word 00001h                ; 3d 01 00
    jc short 062e3h                           ; 72 0b
    cmp ax, strict word 00003h                ; 3d 03 00
    jnbe short 062e3h                         ; 77 06
    mov word [bp+01ch], cx                    ; 89 4e 1c
    jmp near 05f2eh                           ; e9 4b fc
    or byte [bp+01ch], 001h                   ; 80 4e 1c 01
    jmp near 05f2eh                           ; e9 44 fc
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 b9 b5
    push word [bp+010h]                       ; ff 76 10
    push word [bp+016h]                       ; ff 76 16
    push 0084ch                               ; 68 4c 08
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 e6 b5
    add sp, strict byte 00008h                ; 83 c4 08
    jmp short 0628ah                          ; eb 82
    dec dx                                    ; 4a
    and byte [fs:si-05eh], 064h               ; 64 80 64 a2 64
    retn                                      ; c3
    db  064h, 0e2h, 064h
    ; fs loop 06376h                            ; 64 e2 64
    add word [di+043h], sp                    ; 01 65 43
    db  065h, 070h, 065h
    ; gs jo short 0637dh                        ; 65 70 65
_int15_function32:                           ; 0xf6318 LB 0x309
    push si                                   ; 56
    enter 00008h, 000h                        ; c8 08 00 00
    mov ax, word [bp+022h]                    ; 8b 46 22
    shr ax, 008h                              ; c1 e8 08
    cmp ax, 000e8h                            ; 3d e8 00
    je short 06369h                           ; 74 41
    cmp ax, 00086h                            ; 3d 86 00
    jne near 065f4h                           ; 0f 85 c5 02
    sti                                       ; fb
    mov ax, word [bp+01eh]                    ; 8b 46 1e
    mov dx, word [bp+01ah]                    ; 8b 56 1a
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2
    mov ebx, strict dword 00000000fh          ; 66 bb 0f 00 00 00
    db  066h, 033h, 0d2h
    ; xor edx, edx                              ; 66 33 d2
    div ebx                                   ; 66 f7 f3
    db  066h, 08bh, 0c8h
    ; mov ecx, eax                              ; 66 8b c8
    in AL, strict byte 061h                   ; e4 61
    and AL, strict byte 010h                  ; 24 10
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0
    db  066h, 00bh, 0c9h
    ; or ecx, ecx                               ; 66 0b c9
    je near 06366h                            ; 0f 84 0e 00
    in AL, strict byte 061h                   ; e4 61
    and AL, strict byte 010h                  ; 24 10
    db  03ah, 0c4h
    ; cmp al, ah                                ; 3a c4
    je short 06358h                           ; 74 f8
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0
    dec ecx                                   ; 66 49
    jne short 06358h                          ; 75 f2
    leave                                     ; c9
    pop si                                    ; 5e
    retn                                      ; c3
    mov ax, word [bp+022h]                    ; 8b 46 22
    xor ah, ah                                ; 30 e4
    cmp ax, strict word 00020h                ; 3d 20 00
    je short 0637dh                           ; 74 0a
    cmp ax, strict word 00001h                ; 3d 01 00
    je near 065a6h                            ; 0f 84 2c 02
    jmp near 065f4h                           ; e9 77 02
    cmp word [bp+01ch], 0534dh                ; 81 7e 1c 4d 53
    jne near 065f4h                           ; 0f 85 6e 02
    cmp word [bp+01ah], 04150h                ; 81 7e 1a 50 41
    jne near 065f4h                           ; 0f 85 65 02
    mov ax, strict word 00035h                ; b8 35 00
    call 0165ch                               ; e8 c7 b2
    movzx bx, al                              ; 0f b6 d8
    xor dx, dx                                ; 31 d2
    mov cx, strict word 00008h                ; b9 08 00
    sal bx, 1                                 ; d1 e3
    rcl dx, 1                                 ; d1 d2
    loop 0639dh                               ; e2 fa
    mov ax, strict word 00034h                ; b8 34 00
    call 0165ch                               ; e8 b3 b2
    xor ah, ah                                ; 30 e4
    mov dx, bx                                ; 89 da
    or dx, ax                                 ; 09 c2
    xor bx, bx                                ; 31 db
    add bx, bx                                ; 01 db
    adc dx, 00100h                            ; 81 d2 00 01
    cmp dx, 00100h                            ; 81 fa 00 01
    jc short 063c3h                           ; 72 06
    jne short 063f0h                          ; 75 31
    test bx, bx                               ; 85 db
    jnbe short 063f0h                         ; 77 2d
    mov ax, strict word 00031h                ; b8 31 00
    call 0165ch                               ; e8 93 b2
    movzx bx, al                              ; 0f b6 d8
    xor dx, dx                                ; 31 d2
    mov cx, strict word 00008h                ; b9 08 00
    sal bx, 1                                 ; d1 e3
    rcl dx, 1                                 ; d1 d2
    loop 063d1h                               ; e2 fa
    mov ax, strict word 00030h                ; b8 30 00
    call 0165ch                               ; e8 7f b2
    xor ah, ah                                ; 30 e4
    or bx, ax                                 ; 09 c3
    mov cx, strict word 0000ah                ; b9 0a 00
    sal bx, 1                                 ; d1 e3
    rcl dx, 1                                 ; d1 d2
    loop 063e4h                               ; e2 fa
    add bx, strict byte 00000h                ; 83 c3 00
    adc dx, strict byte 00010h                ; 83 d2 10
    mov ax, strict word 00062h                ; b8 62 00
    call 0165ch                               ; e8 66 b2
    xor ah, ah                                ; 30 e4
    mov word [bp-008h], ax                    ; 89 46 f8
    xor al, al                                ; 30 c0
    mov word [bp-006h], ax                    ; 89 46 fa
    mov cx, strict word 00008h                ; b9 08 00
    sal word [bp-008h], 1                     ; d1 66 f8
    rcl word [bp-006h], 1                     ; d1 56 fa
    loop 06403h                               ; e2 f8
    mov ax, strict word 00061h                ; b8 61 00
    call 0165ch                               ; e8 4b b2
    xor ah, ah                                ; 30 e4
    or word [bp-008h], ax                     ; 09 46 f8
    mov ax, word [bp-008h]                    ; 8b 46 f8
    mov word [bp-006h], ax                    ; 89 46 fa
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00
    mov ax, strict word 00063h                ; b8 63 00
    call 0165ch                               ; e8 35 b2
    mov byte [bp-004h], al                    ; 88 46 fc
    mov byte [bp-002h], al                    ; 88 46 fe
    mov ax, word [bp+016h]                    ; 8b 46 16
    cmp ax, strict word 00007h                ; 3d 07 00
    jnbe near 065f4h                          ; 0f 87 bd 01
    mov si, ax                                ; 89 c6
    add si, ax                                ; 01 c6
    mov ax, bx                                ; 89 d8
    add ax, strict word 00000h                ; 05 00 00
    mov cx, dx                                ; 89 d1
    adc cx, strict byte 0ffffh                ; 83 d1 ff
    jmp word [cs:si+06308h]                   ; 2e ff a4 08 63
    push strict byte 00001h                   ; 6a 01
    push dword 000000000h                     ; 66 6a 00
    push strict byte 00009h                   ; 6a 09
    push 0fc00h                               ; 68 00 fc
    mov dx, word [bp+006h]                    ; 8b 56 06
    mov ax, word [bp+026h]                    ; 8b 46 26
    xor bx, bx                                ; 31 db
    xor cx, cx                                ; 31 c9
    call 05dc3h                               ; e8 62 f9
    mov dword [bp+016h], strict dword 000000001h ; 66 c7 46 16 01 00 00 00
    mov dword [bp+022h], strict dword 0534d4150h ; 66 c7 46 22 50 41 4d 53
    mov dword [bp+01eh], strict dword 000000014h ; 66 c7 46 1e 14 00 00 00
    and byte [bp+02ah], 0feh                  ; 80 66 2a fe
    leave                                     ; c9
    pop si                                    ; 5e
    retn                                      ; c3
    push strict byte 00002h                   ; 6a 02
    push dword 000000000h                     ; 66 6a 00
    push strict byte 0000ah                   ; 6a 0a
    push strict byte 00000h                   ; 6a 00
    mov dx, word [bp+006h]                    ; 8b 56 06
    mov ax, word [bp+026h]                    ; 8b 46 26
    mov bx, 0fc00h                            ; bb 00 fc
    mov cx, strict word 00009h                ; b9 09 00
    call 05dc3h                               ; e8 2b f9
    mov dword [bp+016h], strict dword 000000002h ; 66 c7 46 16 02 00 00 00
    jmp short 06469h                          ; eb c7
    push strict byte 00002h                   ; 6a 02
    push dword 000000000h                     ; 66 6a 00
    push strict byte 00010h                   ; 6a 10
    push strict byte 00000h                   ; 6a 00
    mov dx, word [bp+006h]                    ; 8b 56 06
    mov ax, word [bp+026h]                    ; 8b 46 26
    xor bx, bx                                ; 31 db
    mov cx, strict word 0000fh                ; b9 0f 00
    call 05dc3h                               ; e8 0a f9
    mov dword [bp+016h], strict dword 000000003h ; 66 c7 46 16 03 00 00 00
    jmp short 06469h                          ; eb a6
    push strict byte 00001h                   ; 6a 01
    push dword 000000000h                     ; 66 6a 00
    push cx                                   ; 51
    push ax                                   ; 50
    mov dx, word [bp+006h]                    ; 8b 56 06
    mov ax, word [bp+026h]                    ; 8b 46 26
    xor bx, bx                                ; 31 db
    mov cx, strict word 00010h                ; b9 10 00
    call 05dc3h                               ; e8 eb f8
    mov dword [bp+016h], strict dword 000000004h ; 66 c7 46 16 04 00 00 00
    jmp short 06469h                          ; eb 87
    push strict byte 00003h                   ; 6a 03
    push dword 000000000h                     ; 66 6a 00
    push dx                                   ; 52
    push bx                                   ; 53
    mov dx, word [bp+006h]                    ; 8b 56 06
    mov si, word [bp+026h]                    ; 8b 76 26
    mov bx, ax                                ; 89 c3
    mov ax, si                                ; 89 f0
    call 05dc3h                               ; e8 cd f8
    mov dword [bp+016h], strict dword 000000005h ; 66 c7 46 16 05 00 00 00
    jmp near 06469h                           ; e9 68 ff
    push strict byte 00002h                   ; 6a 02
    push dword 000000000h                     ; 66 6a 00
    push dword 000000000h                     ; 66 6a 00
    mov dx, word [bp+006h]                    ; 8b 56 06
    mov ax, word [bp+026h]                    ; 8b 46 26
    xor bx, bx                                ; 31 db
    mov cx, strict word 0fffch                ; b9 fc ff
    call 05dc3h                               ; e8 ac f8
    cmp byte [bp-004h], 000h                  ; 80 7e fc 00
    jne short 06524h                          ; 75 07
    mov ax, word [bp-006h]                    ; 8b 46 fa
    test ax, ax                               ; 85 c0
    je short 0653ah                           ; 74 16
    mov dword [bp+016h], strict dword 000000007h ; 66 c7 46 16 07 00 00 00
    jmp near 06469h                           ; e9 3a ff
    mov dword [bp+016h], strict dword 000000006h ; 66 c7 46 16 06 00 00 00
    jmp near 06469h                           ; e9 2f ff
    mov word [bp+016h], ax                    ; 89 46 16
    mov word [bp+018h], ax                    ; 89 46 18
    jmp near 06469h                           ; e9 26 ff
    push strict byte 00002h                   ; 6a 02
    push dword 000000000h                     ; 66 6a 00
    push dword 000000000h                     ; 66 6a 00
    mov dx, word [bp+006h]                    ; 8b 56 06
    mov ax, word [bp+026h]                    ; 8b 46 26
    xor bx, bx                                ; 31 db
    xor cx, cx                                ; 31 c9
    call 05dc3h                               ; e8 6b f8
    cmp byte [bp-004h], 000h                  ; 80 7e fc 00
    jne short 06565h                          ; 75 07
    mov ax, word [bp-006h]                    ; 8b 46 fa
    test ax, ax                               ; 85 c0
    je short 06567h                           ; 74 02
    jmp short 06524h                          ; eb bd
    mov word [bp+016h], ax                    ; 89 46 16
    mov word [bp+018h], ax                    ; 89 46 18
    jmp near 06469h                           ; e9 f9 fe
    cmp byte [bp-004h], 000h                  ; 80 7e fc 00
    jne short 0657eh                          ; 75 08
    cmp word [bp-006h], strict byte 00000h    ; 83 7e fa 00
    je near 06469h                            ; 0f 84 eb fe
    push strict byte 00001h                   ; 6a 01
    mov al, byte [bp-002h]                    ; 8a 46 fe
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    push strict byte 00001h                   ; 6a 01
    push dword [bp-008h]                      ; 66 ff 76 f8
    mov dx, word [bp+006h]                    ; 8b 56 06
    mov ax, word [bp+026h]                    ; 8b 46 26
    xor bx, bx                                ; 31 db
    xor cx, cx                                ; 31 c9
    call 05dc3h                               ; e8 28 f8
    xor ax, ax                                ; 31 c0
    mov word [bp+016h], ax                    ; 89 46 16
    mov word [bp+018h], ax                    ; 89 46 18
    jmp near 06469h                           ; e9 c3 fe
    and byte [bp+02ah], 0feh                  ; 80 66 2a fe
    mov ax, strict word 00031h                ; b8 31 00
    call 0165ch                               ; e8 ac b0
    xor ah, ah                                ; 30 e4
    mov dx, ax                                ; 89 c2
    sal dx, 008h                              ; c1 e2 08
    mov ax, strict word 00030h                ; b8 30 00
    call 0165ch                               ; e8 9f b0
    xor ah, ah                                ; 30 e4
    or dx, ax                                 ; 09 c2
    mov word [bp+01eh], dx                    ; 89 56 1e
    cmp dx, 03c00h                            ; 81 fa 00 3c
    jbe short 065cfh                          ; 76 05
    mov word [bp+01eh], 03c00h                ; c7 46 1e 00 3c
    mov ax, strict word 00035h                ; b8 35 00
    call 0165ch                               ; e8 87 b0
    movzx dx, al                              ; 0f b6 d0
    sal dx, 008h                              ; c1 e2 08
    mov ax, strict word 00034h                ; b8 34 00
    call 0165ch                               ; e8 7b b0
    xor ah, ah                                ; 30 e4
    or dx, ax                                 ; 09 c2
    mov word [bp+01ah], dx                    ; 89 56 1a
    mov ax, word [bp+01eh]                    ; 8b 46 1e
    mov word [bp+022h], ax                    ; 89 46 22
    mov word [bp+016h], dx                    ; 89 56 16
    leave                                     ; c9
    pop si                                    ; 5e
    retn                                      ; c3
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 af b2
    push word [bp+016h]                       ; ff 76 16
    push word [bp+022h]                       ; ff 76 22
    push 0084ch                               ; 68 4c 08
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 dc b2
    add sp, strict byte 00008h                ; 83 c4 08
    or byte [bp+02ah], 001h                   ; 80 4e 2a 01
    mov ax, word [bp+022h]                    ; 8b 46 22
    xor al, al                                ; 30 c0
    or AL, strict byte 086h                   ; 0c 86
    mov word [bp+022h], ax                    ; 89 46 22
    leave                                     ; c9
    pop si                                    ; 5e
    retn                                      ; c3
init_rtc_:                                   ; 0xf6621 LB 0x25
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, strict word 00026h                ; ba 26 00
    mov ax, strict word 0000ah                ; b8 0a 00
    call 0166dh                               ; e8 3f b0
    mov dx, strict word 00002h                ; ba 02 00
    mov ax, strict word 0000bh                ; b8 0b 00
    call 0166dh                               ; e8 36 b0
    mov ax, strict word 0000ch                ; b8 0c 00
    call 0165ch                               ; e8 1f b0
    mov ax, strict word 0000dh                ; b8 0d 00
    call 0165ch                               ; e8 19 b0
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    retn                                      ; c3
rtc_updating_:                               ; 0xf6646 LB 0x1f
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, 061a8h                            ; ba a8 61
    dec dx                                    ; 4a
    je short 0665fh                           ; 74 0f
    mov ax, strict word 0000ah                ; b8 0a 00
    call 0165ch                               ; e8 06 b0
    test AL, strict byte 080h                 ; a8 80
    jne short 0664dh                          ; 75 f3
    xor ax, ax                                ; 31 c0
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    retn                                      ; c3
    mov ax, strict word 00001h                ; b8 01 00
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    retn                                      ; c3
_int70_function:                             ; 0xf6665 LB 0xbb
    push si                                   ; 56
    enter 00002h, 000h                        ; c8 02 00 00
    mov ax, strict word 0000bh                ; b8 0b 00
    call 0165ch                               ; e8 ec af
    mov dl, al                                ; 88 c2
    mov byte [bp-002h], al                    ; 88 46 fe
    mov ax, strict word 0000ch                ; b8 0c 00
    call 0165ch                               ; e8 e1 af
    mov dh, al                                ; 88 c6
    test dl, 060h                             ; f6 c2 60
    je near 0670ah                            ; 0f 84 86 00
    test AL, strict byte 020h                 ; a8 20
    je short 0668ch                           ; 74 04
    sti                                       ; fb
    int 04ah                                  ; cd 4a
    cli                                       ; fa
    test dh, 040h                             ; f6 c6 40
    je near 0670ah                            ; 0f 84 77 00
    mov dx, 000a0h                            ; ba a0 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 64 af
    test al, al                               ; 84 c0
    je short 0670ah                           ; 74 6a
    mov dx, 0009ch                            ; ba 9c 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01638h                               ; e8 8f af
    test dx, dx                               ; 85 d2
    jne short 066f6h                          ; 75 49
    cmp ax, 003d1h                            ; 3d d1 03
    jnc short 066f6h                          ; 73 44
    mov dx, 00098h                            ; ba 98 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 61 af
    mov si, ax                                ; 89 c6
    mov dx, 0009ah                            ; ba 9a 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 56 af
    mov cx, ax                                ; 89 c1
    xor bx, bx                                ; 31 db
    mov dx, 000a0h                            ; ba a0 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 3b af
    mov al, byte [bp-002h]                    ; 8a 46 fe
    and AL, strict byte 037h                  ; 24 37
    movzx dx, al                              ; 0f b6 d0
    mov ax, strict word 0000bh                ; b8 0b 00
    call 0166dh                               ; e8 8c af
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 01600h                               ; e8 18 af
    or AL, strict byte 080h                   ; 0c 80
    movzx bx, al                              ; 0f b6 d8
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 0160eh                               ; e8 1a af
    jmp short 0670ah                          ; eb 14
    mov bx, ax                                ; 89 c3
    add bx, 0fc2fh                            ; 81 c3 2f fc
    mov cx, dx                                ; 89 d1
    adc cx, strict byte 0ffffh                ; 83 d1 ff
    mov dx, 0009ch                            ; ba 9c 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0164ah                               ; e8 40 af
    call 0e03bh                               ; e8 2e 79
    leave                                     ; c9
    pop si                                    ; 5e
    retn                                      ; c3
    aaa                                       ; 37
    db  067h, 05eh
    ; pop si                                    ; 67 5e
    and word [edi-04bh], strict byte 00067h   ; 67 83 67 b5 67
    add bp, word [bx+si+039h]                 ; 03 68 39
    push 0687bh                               ; 68 7b 68
    db  0d0h
    db  068h
_int1a_function:                             ; 0xf6720 LB 0x1c0
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sti                                       ; fb
    mov al, byte [bp+013h]                    ; 8a 46 13
    cmp AL, strict byte 007h                  ; 3c 07
    jnbe near 0675ch                          ; 0f 87 2f 00
    movzx bx, al                              ; 0f b6 d8
    add bx, bx                                ; 01 db
    jmp word [cs:bx+06710h]                   ; 2e ff a7 10 67
    cli                                       ; fa
    mov bx, 0046eh                            ; bb 6e 04
    xor ax, ax                                ; 31 c0
    mov es, ax                                ; 8e c0
    mov ax, word [es:bx]                      ; 26 8b 07
    mov word [bp+010h], ax                    ; 89 46 10
    mov bx, 0046ch                            ; bb 6c 04
    mov ax, word [es:bx]                      ; 26 8b 07
    mov word [bp+00eh], ax                    ; 89 46 0e
    mov bx, 00470h                            ; bb 70 04
    mov al, byte [es:bx]                      ; 26 8a 07
    mov byte [bp+012h], al                    ; 88 46 12
    mov byte [es:bx], 000h                    ; 26 c6 07 00
    sti                                       ; fb
    pop bp                                    ; 5d
    retn                                      ; c3
    cli                                       ; fa
    mov bx, 0046eh                            ; bb 6e 04
    xor ax, ax                                ; 31 c0
    mov es, ax                                ; 8e c0
    mov ax, word [bp+010h]                    ; 8b 46 10
    mov word [es:bx], ax                      ; 26 89 07
    mov bx, 0046ch                            ; bb 6c 04
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    mov word [es:bx], ax                      ; 26 89 07
    mov bx, 00470h                            ; bb 70 04
    mov byte [es:bx], 000h                    ; 26 c6 07 00
    sti                                       ; fb
    mov byte [bp+013h], 000h                  ; c6 46 13 00
    pop bp                                    ; 5d
    retn                                      ; c3
    call 06646h                               ; e8 c0 fe
    test ax, ax                               ; 85 c0
    je short 0678ch                           ; 74 02
    pop bp                                    ; 5d
    retn                                      ; c3
    xor ax, ax                                ; 31 c0
    call 0165ch                               ; e8 cb ae
    mov byte [bp+00fh], al                    ; 88 46 0f
    mov ax, strict word 00002h                ; b8 02 00
    call 0165ch                               ; e8 c2 ae
    mov byte [bp+010h], al                    ; 88 46 10
    mov ax, strict word 00004h                ; b8 04 00
    call 0165ch                               ; e8 b9 ae
    mov bl, al                                ; 88 c3
    mov byte [bp+011h], al                    ; 88 46 11
    mov ax, strict word 0000bh                ; b8 0b 00
    call 0165ch                               ; e8 ae ae
    and AL, strict byte 001h                  ; 24 01
    mov byte [bp+00eh], al                    ; 88 46 0e
    jmp short 067fah                          ; eb 45
    call 06646h                               ; e8 8e fe
    test ax, ax                               ; 85 c0
    je short 067bfh                           ; 74 03
    call 06621h                               ; e8 62 fe
    movzx dx, byte [bp+00fh]                  ; 0f b6 56 0f
    xor ax, ax                                ; 31 c0
    call 0166dh                               ; e8 a5 ae
    movzx dx, byte [bp+010h]                  ; 0f b6 56 10
    mov ax, strict word 00002h                ; b8 02 00
    call 0166dh                               ; e8 9b ae
    movzx dx, byte [bp+011h]                  ; 0f b6 56 11
    mov ax, strict word 00004h                ; b8 04 00
    call 0166dh                               ; e8 91 ae
    mov ax, strict word 0000bh                ; b8 0b 00
    call 0165ch                               ; e8 7a ae
    mov bl, al                                ; 88 c3
    and bl, 060h                              ; 80 e3 60
    or bl, 002h                               ; 80 cb 02
    mov al, byte [bp+00eh]                    ; 8a 46 0e
    and AL, strict byte 001h                  ; 24 01
    or bl, al                                 ; 08 c3
    movzx dx, bl                              ; 0f b6 d3
    mov ax, strict word 0000bh                ; b8 0b 00
    call 0166dh                               ; e8 73 ae
    mov byte [bp+013h], 000h                  ; c6 46 13 00
    mov byte [bp+012h], bl                    ; 88 5e 12
    pop bp                                    ; 5d
    retn                                      ; c3
    mov byte [bp+013h], 000h                  ; c6 46 13 00
    call 06646h                               ; e8 3c fe
    test ax, ax                               ; 85 c0
    je short 06810h                           ; 74 02
    pop bp                                    ; 5d
    retn                                      ; c3
    mov ax, strict word 00009h                ; b8 09 00
    call 0165ch                               ; e8 46 ae
    mov byte [bp+010h], al                    ; 88 46 10
    mov ax, strict word 00008h                ; b8 08 00
    call 0165ch                               ; e8 3d ae
    mov byte [bp+00fh], al                    ; 88 46 0f
    mov ax, strict word 00007h                ; b8 07 00
    call 0165ch                               ; e8 34 ae
    mov byte [bp+00eh], al                    ; 88 46 0e
    mov ax, strict word 00032h                ; b8 32 00
    call 0165ch                               ; e8 2b ae
    mov byte [bp+011h], al                    ; 88 46 11
    mov byte [bp+012h], al                    ; 88 46 12
    pop bp                                    ; 5d
    retn                                      ; c3
    call 06646h                               ; e8 0a fe
    test ax, ax                               ; 85 c0
    je short 06845h                           ; 74 05
    call 06621h                               ; e8 de fd
    pop bp                                    ; 5d
    retn                                      ; c3
    movzx dx, byte [bp+010h]                  ; 0f b6 56 10
    mov ax, strict word 00009h                ; b8 09 00
    call 0166dh                               ; e8 1e ae
    movzx dx, byte [bp+00fh]                  ; 0f b6 56 0f
    mov ax, strict word 00008h                ; b8 08 00
    call 0166dh                               ; e8 14 ae
    movzx dx, byte [bp+00eh]                  ; 0f b6 56 0e
    mov ax, strict word 00007h                ; b8 07 00
    call 0166dh                               ; e8 0a ae
    movzx dx, byte [bp+011h]                  ; 0f b6 56 11
    mov ax, strict word 00032h                ; b8 32 00
    call 0166dh                               ; e8 00 ae
    mov ax, strict word 0000bh                ; b8 0b 00
    call 0165ch                               ; e8 e9 ad
    mov bl, al                                ; 88 c3
    and bl, 07fh                              ; 80 e3 7f
    jmp near 067f1h                           ; e9 76 ff
    mov ax, strict word 0000bh                ; b8 0b 00
    call 0165ch                               ; e8 db ad
    mov bl, al                                ; 88 c3
    mov word [bp+012h], strict word 00000h    ; c7 46 12 00 00
    test AL, strict byte 020h                 ; a8 20
    je short 0688eh                           ; 74 02
    pop bp                                    ; 5d
    retn                                      ; c3
    call 06646h                               ; e8 b5 fd
    test ax, ax                               ; 85 c0
    je short 06898h                           ; 74 03
    call 06621h                               ; e8 89 fd
    movzx dx, byte [bp+00fh]                  ; 0f b6 56 0f
    mov ax, strict word 00001h                ; b8 01 00
    call 0166dh                               ; e8 cb ad
    movzx dx, byte [bp+010h]                  ; 0f b6 56 10
    mov ax, strict word 00003h                ; b8 03 00
    call 0166dh                               ; e8 c1 ad
    movzx dx, byte [bp+011h]                  ; 0f b6 56 11
    mov ax, strict word 00005h                ; b8 05 00
    call 0166dh                               ; e8 b7 ad
    mov dx, 000a1h                            ; ba a1 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and AL, strict byte 0feh                  ; 24 fe
    out DX, AL                                ; ee
    mov al, bl                                ; 88 d8
    and AL, strict byte 05fh                  ; 24 5f
    or AL, strict byte 020h                   ; 0c 20
    movzx dx, al                              ; 0f b6 d0
    mov ax, strict word 0000bh                ; b8 0b 00
    call 0166dh                               ; e8 9f ad
    pop bp                                    ; 5d
    retn                                      ; c3
    mov ax, strict word 0000bh                ; b8 0b 00
    call 0165ch                               ; e8 86 ad
    mov bl, al                                ; 88 c3
    and AL, strict byte 057h                  ; 24 57
    movzx dx, al                              ; 0f b6 d0
    jmp near 067f4h                           ; e9 14 ff
send_to_mouse_ctrl_:                         ; 0xf68e0 LB 0x31
    push bx                                   ; 53
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bl, al                                ; 88 c3
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 002h                 ; a8 02
    je short 068ffh                           ; 74 0e
    push 00886h                               ; 68 86 08
    push 010fah                               ; 68 fa 10
    push strict byte 00007h                   ; 6a 07
    call 018e9h                               ; e8 ed af
    add sp, strict byte 00006h                ; 83 c4 06
    mov AL, strict byte 0d4h                  ; b0 d4
    mov dx, strict word 00064h                ; ba 64 00
    out DX, AL                                ; ee
    mov al, bl                                ; 88 d8
    mov dx, strict word 00060h                ; ba 60 00
    out DX, AL                                ; ee
    xor al, bl                                ; 30 d8
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    retn                                      ; c3
get_mouse_data_:                             ; 0xf6911 LB 0x38
    push bx                                   ; 53
    push cx                                   ; 51
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, ax                                ; 89 c3
    mov es, dx                                ; 8e c2
    mov cx, 02710h                            ; b9 10 27
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and ax, strict word 00021h                ; 25 21 00
    cmp ax, strict word 00021h                ; 3d 21 00
    je short 06932h                           ; 74 07
    test cx, cx                               ; 85 c9
    je short 06932h                           ; 74 03
    dec cx                                    ; 49
    jmp short 0691dh                          ; eb eb
    test cx, cx                               ; 85 c9
    jne short 0693ah                          ; 75 04
    mov AL, strict byte 001h                  ; b0 01
    jmp short 06945h                          ; eb 0b
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov byte [es:bx], al                      ; 26 88 07
    xor al, al                                ; 30 c0
    pop bp                                    ; 5d
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
set_kbd_command_byte_:                       ; 0xf6949 LB 0x2f
    push bx                                   ; 53
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bl, al                                ; 88 c3
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 002h                 ; a8 02
    je short 06968h                           ; 74 0e
    push 00890h                               ; 68 90 08
    push 010fah                               ; 68 fa 10
    push strict byte 00007h                   ; 6a 07
    call 018e9h                               ; e8 84 af
    add sp, strict byte 00006h                ; 83 c4 06
    mov AL, strict byte 060h                  ; b0 60
    mov dx, strict word 00064h                ; ba 64 00
    out DX, AL                                ; ee
    mov al, bl                                ; 88 d8
    mov dx, strict word 00060h                ; ba 60 00
    out DX, AL                                ; ee
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    retn                                      ; c3
_int74_function:                             ; 0xf6978 LB 0xc6
    enter 00008h, 000h                        ; c8 08 00 00
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 97 ac
    mov cx, ax                                ; 89 c1
    mov word [bp+004h], strict word 00000h    ; c7 46 04 00 00
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and AL, strict byte 021h                  ; 24 21
    cmp AL, strict byte 021h                  ; 3c 21
    jne near 06a2ch                           ; 0f 85 92 00
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov bl, al                                ; 88 c3
    mov dx, strict word 00026h                ; ba 26 00
    mov ax, cx                                ; 89 c8
    call 01600h                               ; e8 56 ac
    mov byte [bp-006h], al                    ; 88 46 fa
    mov dx, strict word 00027h                ; ba 27 00
    mov ax, cx                                ; 89 c8
    call 01600h                               ; e8 4b ac
    mov byte [bp-008h], al                    ; 88 46 f8
    test AL, strict byte 080h                 ; a8 80
    je short 06a2ch                           ; 74 70
    mov al, byte [bp-008h]                    ; 8a 46 f8
    and AL, strict byte 007h                  ; 24 07
    mov byte [bp-002h], al                    ; 88 46 fe
    mov al, byte [bp-006h]                    ; 8a 46 fa
    and AL, strict byte 007h                  ; 24 07
    mov byte [bp-004h], al                    ; 88 46 fc
    xor bh, bh                                ; 30 ff
    movzx dx, al                              ; 0f b6 d0
    add dx, strict byte 00028h                ; 83 c2 28
    mov ax, cx                                ; 89 c8
    call 0160eh                               ; e8 35 ac
    mov al, byte [bp-004h]                    ; 8a 46 fc
    cmp al, byte [bp-002h]                    ; 3a 46 fe
    jc short 06a1dh                           ; 72 3c
    mov dx, strict word 00028h                ; ba 28 00
    mov ax, cx                                ; 89 c8
    call 01600h                               ; e8 17 ac
    xor ah, ah                                ; 30 e4
    mov word [bp+00ch], ax                    ; 89 46 0c
    mov dx, strict word 00029h                ; ba 29 00
    mov ax, cx                                ; 89 c8
    call 01600h                               ; e8 0a ac
    xor ah, ah                                ; 30 e4
    mov word [bp+00ah], ax                    ; 89 46 0a
    mov dx, strict word 0002ah                ; ba 2a 00
    mov ax, cx                                ; 89 c8
    call 01600h                               ; e8 fd ab
    xor ah, ah                                ; 30 e4
    mov word [bp+008h], ax                    ; 89 46 08
    xor al, al                                ; 30 c0
    mov word [bp+006h], ax                    ; 89 46 06
    mov byte [bp-006h], ah                    ; 88 66 fa
    test byte [bp-008h], 080h                 ; f6 46 f8 80
    je short 06a20h                           ; 74 0a
    mov word [bp+004h], strict word 00001h    ; c7 46 04 01 00
    jmp short 06a20h                          ; eb 03
    inc byte [bp-006h]                        ; fe 46 fa
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa
    mov dx, strict word 00026h                ; ba 26 00
    mov ax, cx                                ; 89 c8
    call 0160eh                               ; e8 e2 ab
    leave                                     ; c9
    retn                                      ; c3
    sub byte [bp+si-00ah], 06ah               ; 80 6a f6 6a
    jns short 06a9fh                          ; 79 6b
    or ch, byte [si+078h]                     ; 0a 6c 78
    insb                                      ; 6c
    int3                                      ; cc
    push strict byte 0ffa0h                   ; 6a a0
    insb                                      ; 6c
    db  065h, 06dh
    ; gs insw                                   ; 65 6d
_int15_function_mouse:                       ; 0xf6a3e LB 0x386
    push si                                   ; 56
    enter 00006h, 000h                        ; c8 06 00 00
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 d0 ab
    mov cx, ax                                ; 89 c1
    cmp byte [bp+014h], 007h                  ; 80 7e 14 07
    jbe short 06a5fh                          ; 76 0b
    or word [bp+01ah], strict byte 00001h     ; 83 4e 1a 01
    mov byte [bp+015h], 001h                  ; c6 46 15 01
    leave                                     ; c9
    pop si                                    ; 5e
    retn                                      ; c3
    mov ax, strict word 00065h                ; b8 65 00
    call 06949h                               ; e8 e4 fe
    and word [bp+01ah], strict byte 0fffeh    ; 83 66 1a fe
    mov byte [bp+015h], 000h                  ; c6 46 15 00
    mov al, byte [bp+014h]                    ; 8a 46 14
    cmp AL, strict byte 007h                  ; 3c 07
    jnbe near 06da8h                          ; 0f 87 32 03
    movzx si, al                              ; 0f b6 f0
    add si, si                                ; 01 f6
    jmp word [cs:si+06a2eh]                   ; 2e ff a4 2e 6a
    cmp byte [bp+00fh], 001h                  ; 80 7e 0f 01
    jnbe near 06db3h                          ; 0f 87 2b 03
    mov dx, strict word 00027h                ; ba 27 00
    mov ax, cx                                ; 89 c8
    call 01600h                               ; e8 70 ab
    test AL, strict byte 080h                 ; a8 80
    jne short 06a9fh                          ; 75 0b
    or word [bp+01ah], strict byte 00001h     ; 83 4e 1a 01
    mov byte [bp+015h], 005h                  ; c6 46 15 05
    jmp near 06dbbh                           ; e9 1c 03
    cmp byte [bp+00fh], 000h                  ; 80 7e 0f 00
    db  00fh, 094h, 0c0h
    ; sete al                                   ; 0f 94 c0
    add AL, strict byte 0f4h                  ; 04 f4
    xor ah, ah                                ; 30 e4
    call 068e0h                               ; e8 33 fe
    test al, al                               ; 84 c0
    jne near 06d41h                           ; 0f 85 8e 02
    mov dx, ss                                ; 8c d2
    lea ax, [bp-006h]                         ; 8d 46 fa
    call 06911h                               ; e8 56 fe
    test al, al                               ; 84 c0
    je near 06dbbh                            ; 0f 84 fa 02
    cmp byte [bp-006h], 0fah                  ; 80 7e fa fa
    jne near 06d41h                           ; 0f 85 78 02
    jmp near 06dbbh                           ; e9 ef 02
    mov al, byte [bp+00fh]                    ; 8a 46 0f
    cmp AL, strict byte 001h                  ; 3c 01
    jc short 06ad7h                           ; 72 04
    cmp AL, strict byte 008h                  ; 3c 08
    jbe short 06adah                          ; 76 03
    jmp near 06c6dh                           ; e9 93 01
    mov dx, strict word 00027h                ; ba 27 00
    mov ax, cx                                ; 89 c8
    call 01600h                               ; e8 1e ab
    mov ah, byte [bp+00fh]                    ; 8a 66 0f
    db  0feh, 0cch
    ; dec ah                                    ; fe cc
    and AL, strict byte 0f8h                  ; 24 f8
    or al, ah                                 ; 08 e0
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 00027h                ; ba 27 00
    mov ax, cx                                ; 89 c8
    call 0160eh                               ; e8 18 ab
    mov dx, strict word 00026h                ; ba 26 00
    mov ax, cx                                ; 89 c8
    call 01600h                               ; e8 02 ab
    and AL, strict byte 0f8h                  ; 24 f8
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 00026h                ; ba 26 00
    mov ax, cx                                ; 89 c8
    call 0160eh                               ; e8 03 ab
    mov ax, 000ffh                            ; b8 ff 00
    call 068e0h                               ; e8 cf fd
    test al, al                               ; 84 c0
    jne near 06d41h                           ; 0f 85 2a 02
    mov dx, ss                                ; 8c d2
    lea ax, [bp-002h]                         ; 8d 46 fe
    call 06911h                               ; e8 f2 fd
    mov cl, al                                ; 88 c1
    cmp byte [bp-002h], 0feh                  ; 80 7e fe fe
    jne short 06b32h                          ; 75 0b
    or word [bp+01ah], strict byte 00001h     ; 83 4e 1a 01
    mov byte [bp+015h], 004h                  ; c6 46 15 04
    jmp near 06dbbh                           ; e9 89 02
    cmp byte [bp-002h], 0fah                  ; 80 7e fe fa
    je short 06b48h                           ; 74 10
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    push ax                                   ; 50
    push 0089bh                               ; 68 9b 08
    push strict byte 00007h                   ; 6a 07
    call 018e9h                               ; e8 a4 ad
    add sp, strict byte 00006h                ; 83 c4 06
    test cl, cl                               ; 84 c9
    jne near 06d41h                           ; 0f 85 f3 01
    mov dx, ss                                ; 8c d2
    lea ax, [bp-006h]                         ; 8d 46 fa
    call 06911h                               ; e8 bb fd
    test al, al                               ; 84 c0
    jne near 06d41h                           ; 0f 85 e5 01
    mov dx, ss                                ; 8c d2
    lea ax, [bp-004h]                         ; 8d 46 fc
    call 06911h                               ; e8 ad fd
    test al, al                               ; 84 c0
    jne near 06d41h                           ; 0f 85 d7 01
    mov al, byte [bp-006h]                    ; 8a 46 fa
    mov byte [bp+00eh], al                    ; 88 46 0e
    mov al, byte [bp-004h]                    ; 8a 46 fc
    mov byte [bp+00fh], al                    ; 88 46 0f
    jmp near 06dbbh                           ; e9 42 02
    mov al, byte [bp+00fh]                    ; 8a 46 0f
    cmp AL, strict byte 003h                  ; 3c 03
    jc short 06b90h                           ; 72 10
    jbe short 06baeh                          ; 76 2c
    cmp AL, strict byte 006h                  ; 3c 06
    je short 06bc0h                           ; 74 3a
    cmp AL, strict byte 005h                  ; 3c 05
    je short 06bbah                           ; 74 30
    cmp AL, strict byte 004h                  ; 3c 04
    je short 06bb4h                           ; 74 26
    jmp short 06bc6h                          ; eb 36
    cmp AL, strict byte 002h                  ; 3c 02
    je short 06ba8h                           ; 74 14
    cmp AL, strict byte 001h                  ; 3c 01
    je short 06ba2h                           ; 74 0a
    test al, al                               ; 84 c0
    jne short 06bc6h                          ; 75 2a
    mov byte [bp-006h], 00ah                  ; c6 46 fa 0a
    jmp short 06bcah                          ; eb 28
    mov byte [bp-006h], 014h                  ; c6 46 fa 14
    jmp short 06bcah                          ; eb 22
    mov byte [bp-006h], 028h                  ; c6 46 fa 28
    jmp short 06bcah                          ; eb 1c
    mov byte [bp-006h], 03ch                  ; c6 46 fa 3c
    jmp short 06bcah                          ; eb 16
    mov byte [bp-006h], 050h                  ; c6 46 fa 50
    jmp short 06bcah                          ; eb 10
    mov byte [bp-006h], 064h                  ; c6 46 fa 64
    jmp short 06bcah                          ; eb 0a
    mov byte [bp-006h], 0c8h                  ; c6 46 fa c8
    jmp short 06bcah                          ; eb 04
    mov byte [bp-006h], 000h                  ; c6 46 fa 00
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00
    jbe short 06bffh                          ; 76 2f
    mov ax, 000f3h                            ; b8 f3 00
    call 068e0h                               ; e8 0a fd
    test al, al                               ; 84 c0
    jne short 06bf4h                          ; 75 1a
    mov dx, ss                                ; 8c d2
    lea ax, [bp-004h]                         ; 8d 46 fc
    call 06911h                               ; e8 2f fd
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa
    call 068e0h                               ; e8 f7 fc
    mov dx, ss                                ; 8c d2
    lea ax, [bp-004h]                         ; 8d 46 fc
    call 06911h                               ; e8 20 fd
    jmp near 06dbbh                           ; e9 c7 01
    or word [bp+01ah], strict byte 00001h     ; 83 4e 1a 01
    mov byte [bp+015h], 003h                  ; c6 46 15 03
    jmp near 06dbbh                           ; e9 bc 01
    or word [bp+01ah], strict byte 00001h     ; 83 4e 1a 01
    mov byte [bp+015h], 002h                  ; c6 46 15 02
    jmp near 06dbbh                           ; e9 b1 01
    cmp byte [bp+00fh], 004h                  ; 80 7e 0f 04
    jnc short 06c6dh                          ; 73 5d
    mov ax, 000e8h                            ; b8 e8 00
    call 068e0h                               ; e8 ca fc
    test al, al                               ; 84 c0
    jne short 06c62h                          ; 75 48
    mov dx, ss                                ; 8c d2
    lea ax, [bp-006h]                         ; 8d 46 fa
    call 06911h                               ; e8 ef fc
    cmp byte [bp-006h], 0fah                  ; 80 7e fa fa
    je short 06c38h                           ; 74 10
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa
    push ax                                   ; 50
    push 008c6h                               ; 68 c6 08
    push strict byte 00007h                   ; 6a 07
    call 018e9h                               ; e8 b4 ac
    add sp, strict byte 00006h                ; 83 c4 06
    movzx ax, byte [bp+00fh]                  ; 0f b6 46 0f
    call 068e0h                               ; e8 a1 fc
    mov dx, ss                                ; 8c d2
    lea ax, [bp-006h]                         ; 8d 46 fa
    call 06911h                               ; e8 ca fc
    cmp byte [bp-006h], 0fah                  ; 80 7e fa fa
    je near 06dbbh                            ; 0f 84 6c 01
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa
    push ax                                   ; 50
    push 008c6h                               ; 68 c6 08
    push strict byte 00007h                   ; 6a 07
    call 018e9h                               ; e8 8d ac
    add sp, strict byte 00006h                ; 83 c4 06
    jmp near 06dbbh                           ; e9 59 01
    or word [bp+01ah], strict byte 00001h     ; 83 4e 1a 01
    mov byte [bp+015h], 003h                  ; c6 46 15 03
    jmp near 06dbbh                           ; e9 4e 01
    or word [bp+01ah], strict byte 00001h     ; 83 4e 1a 01
    mov byte [bp+015h], 002h                  ; c6 46 15 02
    jmp near 06dbbh                           ; e9 43 01
    mov ax, 000f2h                            ; b8 f2 00
    call 068e0h                               ; e8 62 fc
    test al, al                               ; 84 c0
    jne short 06c95h                          ; 75 13
    mov dx, ss                                ; 8c d2
    lea ax, [bp-006h]                         ; 8d 46 fa
    call 06911h                               ; e8 87 fc
    mov dx, ss                                ; 8c d2
    lea ax, [bp-004h]                         ; 8d 46 fc
    call 06911h                               ; e8 7f fc
    jmp near 06b70h                           ; e9 db fe
    or word [bp+01ah], strict byte 00001h     ; 83 4e 1a 01
    mov byte [bp+015h], 003h                  ; c6 46 15 03
    jmp near 06dbbh                           ; e9 1b 01
    mov al, byte [bp+00fh]                    ; 8a 46 0f
    test al, al                               ; 84 c0
    jbe short 06caeh                          ; 76 07
    cmp AL, strict byte 002h                  ; 3c 02
    jbe short 06d17h                          ; 76 6c
    jmp near 06d4bh                           ; e9 9d 00
    mov ax, 000e9h                            ; b8 e9 00
    call 068e0h                               ; e8 2c fc
    test al, al                               ; 84 c0
    jne near 06d41h                           ; 0f 85 87 00
    mov dx, ss                                ; 8c d2
    lea ax, [bp-006h]                         ; 8d 46 fa
    call 06911h                               ; e8 4f fc
    mov cl, al                                ; 88 c1
    cmp byte [bp-006h], 0fah                  ; 80 7e fa fa
    je short 06cdah                           ; 74 10
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa
    push ax                                   ; 50
    push 008c6h                               ; 68 c6 08
    push strict byte 00007h                   ; 6a 07
    call 018e9h                               ; e8 12 ac
    add sp, strict byte 00006h                ; 83 c4 06
    test cl, cl                               ; 84 c9
    jne short 06d41h                          ; 75 63
    mov dx, ss                                ; 8c d2
    lea ax, [bp-006h]                         ; 8d 46 fa
    call 06911h                               ; e8 2b fc
    test al, al                               ; 84 c0
    jne short 06d41h                          ; 75 57
    mov dx, ss                                ; 8c d2
    lea ax, [bp-004h]                         ; 8d 46 fc
    call 06911h                               ; e8 1f fc
    test al, al                               ; 84 c0
    jne short 06d41h                          ; 75 4b
    mov dx, ss                                ; 8c d2
    lea ax, [bp-002h]                         ; 8d 46 fe
    call 06911h                               ; e8 13 fc
    test al, al                               ; 84 c0
    jne short 06d41h                          ; 75 3f
    mov al, byte [bp-006h]                    ; 8a 46 fa
    mov byte [bp+00eh], al                    ; 88 46 0e
    mov al, byte [bp-004h]                    ; 8a 46 fc
    mov byte [bp+012h], al                    ; 88 46 12
    mov al, byte [bp-002h]                    ; 8a 46 fe
    mov byte [bp+010h], al                    ; 88 46 10
    jmp near 06dbbh                           ; e9 a4 00
    cmp AL, strict byte 001h                  ; 3c 01
    jne short 06d20h                          ; 75 05
    mov ax, 000e6h                            ; b8 e6 00
    jmp short 06d23h                          ; eb 03
    mov ax, 000e7h                            ; b8 e7 00
    call 068e0h                               ; e8 ba fb
    mov cl, al                                ; 88 c1
    test cl, cl                               ; 84 c9
    jne short 06d3bh                          ; 75 0f
    mov dx, ss                                ; 8c d2
    lea ax, [bp-006h]                         ; 8d 46 fa
    call 06911h                               ; e8 dd fb
    cmp byte [bp-006h], 0fah                  ; 80 7e fa fa
    db  00fh, 095h, 0c1h
    ; setne cl                                  ; 0f 95 c1
    test cl, cl                               ; 84 c9
    je near 06dbbh                            ; 0f 84 7a 00
    or word [bp+01ah], strict byte 00001h     ; 83 4e 1a 01
    mov byte [bp+015h], 003h                  ; c6 46 15 03
    jmp short 06dbbh                          ; eb 70
    movzx ax, byte [bp+00fh]                  ; 0f b6 46 0f
    push ax                                   ; 50
    push 008f2h                               ; 68 f2 08
    push strict byte 00007h                   ; 6a 07
    call 018e9h                               ; e8 91 ab
    add sp, strict byte 00006h                ; 83 c4 06
    or word [bp+01ah], strict byte 00001h     ; 83 4e 1a 01
    mov byte [bp+015h], 001h                  ; c6 46 15 01
    jmp short 06dbbh                          ; eb 56
    mov si, word [bp+00eh]                    ; 8b 76 0e
    mov bx, si                                ; 89 f3
    mov dx, strict word 00022h                ; ba 22 00
    mov ax, cx                                ; 89 c8
    call 0162ah                               ; e8 b8 a8
    mov bx, word [bp+016h]                    ; 8b 5e 16
    mov dx, strict word 00024h                ; ba 24 00
    mov ax, cx                                ; 89 c8
    call 0162ah                               ; e8 ad a8
    mov dx, strict word 00027h                ; ba 27 00
    mov ax, cx                                ; 89 c8
    call 01600h                               ; e8 7b a8
    mov ah, al                                ; 88 c4
    test si, si                               ; 85 f6
    jne short 06d99h                          ; 75 0e
    cmp word [bp+016h], strict byte 00000h    ; 83 7e 16 00
    jne short 06d99h                          ; 75 08
    test AL, strict byte 080h                 ; a8 80
    je short 06d9bh                           ; 74 06
    and AL, strict byte 07fh                  ; 24 7f
    jmp short 06d9bh                          ; eb 02
    or AL, strict byte 080h                   ; 0c 80
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 00027h                ; ba 27 00
    mov ax, cx                                ; 89 c8
    call 0160eh                               ; e8 68 a8
    jmp short 06dbbh                          ; eb 13
    push 0090ch                               ; 68 0c 09
    push strict byte 00007h                   ; 6a 07
    call 018e9h                               ; e8 39 ab
    add sp, strict byte 00004h                ; 83 c4 04
    or word [bp+01ah], strict byte 00001h     ; 83 4e 1a 01
    mov byte [bp+015h], 001h                  ; c6 46 15 01
    mov ax, strict word 00047h                ; b8 47 00
    call 06949h                               ; e8 88 fb
    leave                                     ; c9
    pop si                                    ; 5e
    retn                                      ; c3
_int17_function:                             ; 0xf6dc4 LB 0xb1
    push si                                   ; 56
    enter 00002h, 000h                        ; c8 02 00 00
    sti                                       ; fb
    mov dx, word [bp+010h]                    ; 8b 56 10
    add dx, dx                                ; 01 d2
    add dx, strict byte 00008h                ; 83 c2 08
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 44 a8
    mov bx, ax                                ; 89 c3
    mov si, ax                                ; 89 c6
    cmp byte [bp+015h], 003h                  ; 80 7e 15 03
    jnc near 06e6eh                           ; 0f 83 8a 00
    mov ax, word [bp+010h]                    ; 8b 46 10
    cmp ax, strict word 00003h                ; 3d 03 00
    jnc near 06e6eh                           ; 0f 83 80 00
    test bx, bx                               ; 85 db
    jbe near 06e6eh                           ; 0f 86 7a 00
    mov dx, ax                                ; 89 c2
    add dx, strict byte 00078h                ; 83 c2 78
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 01 a8
    movzx cx, al                              ; 0f b6 c8
    sal cx, 008h                              ; c1 e1 08
    cmp byte [bp+015h], 000h                  ; 80 7e 15 00
    jne short 06e38h                          ; 75 2d
    mov al, byte [bp+014h]                    ; 8a 46 14
    mov dx, bx                                ; 89 da
    out DX, AL                                ; ee
    lea dx, [bx+002h]                         ; 8d 57 02
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov word [bp-002h], ax                    ; 89 46 fe
    mov al, byte [bp-002h]                    ; 8a 46 fe
    or AL, strict byte 001h                   ; 0c 01
    out DX, AL                                ; ee
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    and AL, strict byte 0feh                  ; 24 fe
    out DX, AL                                ; ee
    lea dx, [si+001h]                         ; 8d 54 01
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 040h                 ; a8 40
    je short 06e38h                           ; 74 07
    test cx, cx                               ; 85 c9
    je short 06e38h                           ; 74 03
    dec cx                                    ; 49
    jmp short 06e27h                          ; eb ef
    cmp byte [bp+015h], 001h                  ; 80 7e 15 01
    jne short 06e54h                          ; 75 16
    lea dx, [si+002h]                         ; 8d 54 02
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov word [bp-002h], ax                    ; 89 46 fe
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    and AL, strict byte 0fbh                  ; 24 fb
    out DX, AL                                ; ee
    mov al, byte [bp-002h]                    ; 8a 46 fe
    or AL, strict byte 004h                   ; 0c 04
    out DX, AL                                ; ee
    lea dx, [si+001h]                         ; 8d 54 01
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    xor AL, strict byte 048h                  ; 34 48
    mov byte [bp+015h], al                    ; 88 46 15
    test cx, cx                               ; 85 c9
    jne short 06e67h                          ; 75 04
    or byte [bp+015h], 001h                   ; 80 4e 15 01
    and byte [bp+01eh], 0feh                  ; 80 66 1e fe
    leave                                     ; c9
    pop si                                    ; 5e
    retn                                      ; c3
    or byte [bp+01eh], 001h                   ; 80 4e 1e 01
    leave                                     ; c9
    pop si                                    ; 5e
    retn                                      ; c3
wait_:                                       ; 0xf6e75 LB 0xad
    push bx                                   ; 53
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    enter 0000ah, 000h                        ; c8 0a 00 00
    mov si, ax                                ; 89 c6
    mov byte [bp-004h], dl                    ; 88 56 fc
    mov byte [bp-002h], 000h                  ; c6 46 fe 00
    pushfw                                    ; 9c
    pop ax                                    ; 58
    mov word [bp-008h], ax                    ; 89 46 f8
    sti                                       ; fb
    xor cx, cx                                ; 31 c9
    mov dx, 0046ch                            ; ba 6c 04
    xor ax, ax                                ; 31 c0
    call 01638h                               ; e8 a2 a7
    mov word [bp-006h], ax                    ; 89 46 fa
    mov bx, dx                                ; 89 d3
    hlt                                       ; f4
    mov dx, 0046ch                            ; ba 6c 04
    xor ax, ax                                ; 31 c0
    call 01638h                               ; e8 94 a7
    mov word [bp-00ah], ax                    ; 89 46 f6
    mov di, dx                                ; 89 d7
    cmp dx, bx                                ; 39 da
    jnbe short 06eb4h                         ; 77 07
    jne short 06ebbh                          ; 75 0c
    cmp ax, word [bp-006h]                    ; 3b 46 fa
    jbe short 06ebbh                          ; 76 07
    sub ax, word [bp-006h]                    ; 2b 46 fa
    sbb dx, bx                                ; 19 da
    jmp short 06ec6h                          ; eb 0b
    cmp dx, bx                                ; 39 da
    jc short 06ec6h                           ; 72 07
    jne short 06ecah                          ; 75 09
    cmp ax, word [bp-006h]                    ; 3b 46 fa
    jnc short 06ecah                          ; 73 04
    sub si, ax                                ; 29 c6
    sbb cx, dx                                ; 19 d1
    mov ax, word [bp-00ah]                    ; 8b 46 f6
    mov word [bp-006h], ax                    ; 89 46 fa
    mov bx, di                                ; 89 fb
    mov ax, 00100h                            ; b8 00 01
    int 016h                                  ; cd 16
    je near 06ee0h                            ; 0f 84 05 00
    mov AL, strict byte 001h                  ; b0 01
    jmp near 06ee2h                           ; e9 02 00
    db  032h, 0c0h
    ; xor al, al                                ; 32 c0
    test al, al                               ; 84 c0
    je short 06f0ah                           ; 74 24
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    int 016h                                  ; cd 16
    xchg ah, al                               ; 86 c4
    mov dl, al                                ; 88 c2
    mov byte [bp-002h], al                    ; 88 46 fe
    movzx ax, dl                              ; 0f b6 c2
    push ax                                   ; 50
    push 0092eh                               ; 68 2e 09
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 ec a9
    add sp, strict byte 00006h                ; 83 c4 06
    cmp byte [bp-004h], 000h                  ; 80 7e fc 00
    je short 06f0ah                           ; 74 04
    mov al, dl                                ; 88 d0
    jmp short 06f1ch                          ; eb 12
    test cx, cx                               ; 85 c9
    jnle short 06e9bh                         ; 7f 8d
    jne short 06f14h                          ; 75 04
    test si, si                               ; 85 f6
    jnbe short 06e9bh                         ; 77 87
    mov ax, word [bp-008h]                    ; 8b 46 f8
    push ax                                   ; 50
    popfw                                     ; 9d
    mov al, byte [bp-002h]                    ; 8a 46 fe
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
read_logo_byte_:                             ; 0xf6f22 LB 0x13
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    xor ah, ah                                ; 30 e4
    or ah, 001h                               ; 80 cc 01
    mov dx, 003b8h                            ; ba b8 03
    out DX, ax                                ; ef
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    retn                                      ; c3
read_logo_word_:                             ; 0xf6f35 LB 0x11
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    xor ah, ah                                ; 30 e4
    or ah, 001h                               ; 80 cc 01
    mov dx, 003b8h                            ; ba b8 03
    out DX, ax                                ; ef
    in ax, DX                                 ; ed
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    retn                                      ; c3
print_detected_harddisks_:                   ; 0xf6f46 LB 0x12c
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push si                                   ; 56
    push di                                   ; 57
    enter 00004h, 000h                        ; c8 04 00 00
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 c4 a6
    mov si, ax                                ; 89 c6
    mov byte [bp-002h], 000h                  ; c6 46 fe 00
    xor cl, cl                                ; 30 c9
    xor ch, ch                                ; 30 ed
    mov dx, 002c0h                            ; ba c0 02
    call 01600h                               ; e8 98 a6
    mov byte [bp-004h], al                    ; 88 46 fc
    xor bl, bl                                ; 30 db
    cmp bl, byte [bp-004h]                    ; 3a 5e fc
    jnc near 07047h                           ; 0f 83 d3 00
    movzx dx, bl                              ; 0f b6 d3
    add dx, 002c1h                            ; 81 c2 c1 02
    mov ax, si                                ; 89 f0
    call 01600h                               ; e8 80 a6
    mov bh, al                                ; 88 c7
    cmp AL, strict byte 00ch                  ; 3c 0c
    jc short 06faah                           ; 72 24
    test cl, cl                               ; 84 c9
    jne short 06f97h                          ; 75 0d
    push 0093fh                               ; 68 3f 09
    push strict byte 00002h                   ; 6a 02
    call 018e9h                               ; e8 57 a9
    add sp, strict byte 00004h                ; 83 c4 04
    mov CL, strict byte 001h                  ; b1 01
    movzx ax, bl                              ; 0f b6 c3
    inc ax                                    ; 40
    push ax                                   ; 50
    push 00953h                               ; 68 53 09
    push strict byte 00002h                   ; 6a 02
    call 018e9h                               ; e8 45 a9
    add sp, strict byte 00006h                ; 83 c4 06
    jmp near 07042h                           ; e9 98 00
    cmp AL, strict byte 008h                  ; 3c 08
    jc short 06fc1h                           ; 72 13
    test ch, ch                               ; 84 ed
    jne short 06fbfh                          ; 75 0d
    push 00966h                               ; 68 66 09
    push strict byte 00002h                   ; 6a 02
    call 018e9h                               ; e8 2f a9
    add sp, strict byte 00004h                ; 83 c4 04
    mov CH, strict byte 001h                  ; b5 01
    jmp short 06f97h                          ; eb d6
    cmp AL, strict byte 004h                  ; 3c 04
    jnc short 06fdch                          ; 73 17
    cmp byte [bp-002h], 000h                  ; 80 7e fe 00
    jne short 06fdch                          ; 75 11
    push 0097ah                               ; 68 7a 09
    push strict byte 00002h                   ; 6a 02
    call 018e9h                               ; e8 16 a9
    add sp, strict byte 00004h                ; 83 c4 04
    mov byte [bp-002h], 001h                  ; c6 46 fe 01
    jmp short 06ff2h                          ; eb 16
    cmp bh, 004h                              ; 80 ff 04
    jc short 06ff2h                           ; 72 11
    test cl, cl                               ; 84 c9
    jne short 06ff2h                          ; 75 0d
    push 0093fh                               ; 68 3f 09
    push strict byte 00002h                   ; 6a 02
    call 018e9h                               ; e8 fc a8
    add sp, strict byte 00004h                ; 83 c4 04
    mov CL, strict byte 001h                  ; b1 01
    movzx ax, bl                              ; 0f b6 c3
    inc ax                                    ; 40
    push ax                                   ; 50
    push 0098bh                               ; 68 8b 09
    push strict byte 00002h                   ; 6a 02
    call 018e9h                               ; e8 ea a8
    add sp, strict byte 00006h                ; 83 c4 06
    cmp bh, 004h                              ; 80 ff 04
    jc short 0700ah                           ; 72 03
    sub bh, 004h                              ; 80 ef 04
    movzx ax, bh                              ; 0f b6 c7
    cwd                                       ; 99
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2
    sar ax, 1                                 ; d1 f8
    test ax, ax                               ; 85 c0
    je short 0701bh                           ; 74 05
    push 00995h                               ; 68 95 09
    jmp short 0701eh                          ; eb 03
    push 009a0h                               ; 68 a0 09
    push strict byte 00002h                   ; 6a 02
    call 018e9h                               ; e8 c6 a8
    add sp, strict byte 00004h                ; 83 c4 04
    movzx ax, bh                              ; 0f b6 c7
    mov di, strict word 00002h                ; bf 02 00
    cwd                                       ; 99
    idiv di                                   ; f7 ff
    test dx, dx                               ; 85 d2
    je short 07038h                           ; 74 05
    push 009a9h                               ; 68 a9 09
    jmp short 0703bh                          ; eb 03
    push 009afh                               ; 68 af 09
    push di                                   ; 57
    call 018e9h                               ; e8 aa a8
    add sp, strict byte 00004h                ; 83 c4 04
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3
    jmp near 06f6dh                           ; e9 26 ff
    cmp byte [bp-002h], 000h                  ; 80 7e fe 00
    jne short 07060h                          ; 75 13
    test cl, cl                               ; 84 c9
    jne short 07060h                          ; 75 0f
    test ch, ch                               ; 84 ed
    jne short 07060h                          ; 75 0b
    push 009b6h                               ; 68 b6 09
    push strict byte 00002h                   ; 6a 02
    call 018e9h                               ; e8 8c a8
    add sp, strict byte 00004h                ; 83 c4 04
    push 009cah                               ; 68 ca 09
    push strict byte 00002h                   ; 6a 02
    call 018e9h                               ; e8 81 a8
    add sp, strict byte 00004h                ; 83 c4 04
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
get_boot_drive_:                             ; 0xf7072 LB 0x25
    push bx                                   ; 53
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bl, al                                ; 88 c3
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 9a a5
    mov dx, 002c0h                            ; ba c0 02
    call 01600h                               ; e8 78 a5
    sub bl, 002h                              ; 80 eb 02
    cmp bl, al                                ; 38 c3
    jc short 07091h                           ; 72 02
    mov BL, strict byte 0ffh                  ; b3 ff
    mov al, bl                                ; 88 d8
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    retn                                      ; c3
show_logo_:                                  ; 0xf7097 LB 0x21f
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push si                                   ; 56
    push di                                   ; 57
    enter 0000ch, 000h                        ; c8 0c 00 00
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 73 a5
    mov si, ax                                ; 89 c6
    xor cl, cl                                ; 30 c9
    xor dx, dx                                ; 31 d2
    mov AL, strict byte 034h                  ; b0 34
    out strict byte 043h, AL                  ; e6 43
    mov AL, strict byte 0d3h                  ; b0 d3
    out strict byte 040h, AL                  ; e6 40
    mov AL, strict byte 048h                  ; b0 48
    out strict byte 040h, AL                  ; e6 40
    movzx ax, dl                              ; 0f b6 c2
    call 06f35h                               ; e8 74 fe
    cmp ax, 066bbh                            ; 3d bb 66
    jne near 07199h                           ; 0f 85 d1 00
    push SS                                   ; 16
    pop ES                                    ; 07
    lea di, [bp-00ch]                         ; 8d 7e f4
    mov ax, 04f03h                            ; b8 03 4f
    int 010h                                  ; cd 10
    mov word [es:di], bx                      ; 26 89 1d
    cmp ax, strict word 0004fh                ; 3d 4f 00
    jne near 07199h                           ; 0f 85 bd 00
    mov al, dl                                ; 88 d0
    add AL, strict byte 004h                  ; 04 04
    xor ah, ah                                ; 30 e4
    call 06f22h                               ; e8 3d fe
    mov ch, al                                ; 88 c5
    mov byte [bp-002h], al                    ; 88 46 fe
    mov al, dl                                ; 88 d0
    add AL, strict byte 005h                  ; 04 05
    xor ah, ah                                ; 30 e4
    call 06f22h                               ; e8 2f fe
    mov dh, al                                ; 88 c6
    mov byte [bp-006h], al                    ; 88 46 fa
    mov al, dl                                ; 88 d0
    add AL, strict byte 002h                  ; 04 02
    xor ah, ah                                ; 30 e4
    call 06f35h                               ; e8 34 fe
    mov bx, ax                                ; 89 c3
    mov word [bp-00ah], ax                    ; 89 46 f6
    mov al, dl                                ; 88 d0
    add AL, strict byte 006h                  ; 04 06
    xor ah, ah                                ; 30 e4
    call 06f22h                               ; e8 13 fe
    mov byte [bp-008h], al                    ; 88 46 f8
    test ch, ch                               ; 84 ed
    jne short 07120h                          ; 75 0a
    test dh, dh                               ; 84 f6
    jne short 07120h                          ; 75 06
    test bx, bx                               ; 85 db
    je near 07199h                            ; 0f 84 79 00
    mov bx, 00142h                            ; bb 42 01
    mov ax, 04f02h                            ; b8 02 4f
    int 010h                                  ; cd 10
    cmp byte [bp-002h], 000h                  ; 80 7e fe 00
    je short 07151h                           ; 74 23
    xor bx, bx                                ; 31 db
    jmp short 07138h                          ; eb 06
    inc bx                                    ; 43
    cmp bx, strict byte 00010h                ; 83 fb 10
    jnbe short 07158h                         ; 77 20
    mov ax, bx                                ; 89 d8
    or ah, 002h                               ; 80 cc 02
    mov dx, 003b8h                            ; ba b8 03
    out DX, ax                                ; ef
    xor dx, dx                                ; 31 d2
    mov ax, strict word 00001h                ; b8 01 00
    call 06e75h                               ; e8 2c fd
    cmp AL, strict byte 086h                  ; 3c 86
    jne short 07132h                          ; 75 e5
    mov CL, strict byte 001h                  ; b1 01
    jmp short 07158h                          ; eb 07
    mov ax, 00210h                            ; b8 10 02
    mov dx, 003b8h                            ; ba b8 03
    out DX, ax                                ; ef
    test cl, cl                               ; 84 c9
    jne short 0716eh                          ; 75 12
    mov ax, word [bp-00ah]                    ; 8b 46 f6
    shr ax, 004h                              ; c1 e8 04
    mov dx, strict word 00001h                ; ba 01 00
    call 06e75h                               ; e8 0d fd
    cmp AL, strict byte 086h                  ; 3c 86
    jne short 0716eh                          ; 75 02
    mov CL, strict byte 001h                  ; b1 01
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00
    je short 07199h                           ; 74 25
    test cl, cl                               ; 84 c9
    jne short 07199h                          ; 75 21
    mov bx, strict word 00010h                ; bb 10 00
    jmp short 07182h                          ; eb 05
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 07199h                          ; 76 17
    mov ax, bx                                ; 89 d8
    or ah, 002h                               ; 80 cc 02
    mov dx, 003b8h                            ; ba b8 03
    out DX, ax                                ; ef
    xor dx, dx                                ; 31 d2
    mov ax, strict word 00001h                ; b8 01 00
    call 06e75h                               ; e8 e2 fc
    cmp AL, strict byte 086h                  ; 3c 86
    jne short 0717dh                          ; 75 e6
    mov CL, strict byte 001h                  ; b1 01
    xor bx, bx                                ; 31 db
    mov dx, 00339h                            ; ba 39 03
    mov ax, si                                ; 89 f0
    call 0160eh                               ; e8 6b a4
    mov AL, strict byte 003h                  ; b0 03
    mov AH, strict byte 000h                  ; b4 00
    int 010h                                  ; cd 10
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00
    je near 0729ah                            ; 0f 84 e9 00
    cmp byte [bp-002h], 000h                  ; 80 7e fe 00
    jne short 071e7h                          ; 75 30
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00
    jne short 071e7h                          ; 75 2a
    cmp word [bp-00ah], strict byte 00000h    ; 83 7e f6 00
    jne short 071e7h                          ; 75 24
    cmp byte [bp-008h], 002h                  ; 80 7e f8 02
    jne short 071d4h                          ; 75 0b
    push 009cch                               ; 68 cc 09
    push strict byte 00002h                   ; 6a 02
    call 018e9h                               ; e8 18 a7
    add sp, strict byte 00004h                ; 83 c4 04
    test cl, cl                               ; 84 c9
    jne short 071e7h                          ; 75 0f
    mov dx, strict word 00001h                ; ba 01 00
    mov ax, 000c0h                            ; b8 c0 00
    call 06e75h                               ; e8 94 fc
    cmp AL, strict byte 086h                  ; 3c 86
    jne short 071e7h                          ; 75 02
    mov CL, strict byte 001h                  ; b1 01
    test cl, cl                               ; 84 c9
    je near 0729ah                            ; 0f 84 ad 00
    mov byte [bp-004h], 000h                  ; c6 46 fc 00
    mov ax, 00100h                            ; b8 00 01
    mov cx, 01000h                            ; b9 00 10
    int 010h                                  ; cd 10
    mov ax, 00700h                            ; b8 00 07
    mov BH, strict byte 007h                  ; b7 07
    db  033h, 0c9h
    ; xor cx, cx                                ; 33 c9
    mov dx, 0184fh                            ; ba 4f 18
    int 010h                                  ; cd 10
    mov ax, 00200h                            ; b8 00 02
    db  033h, 0dbh
    ; xor bx, bx                                ; 33 db
    db  033h, 0d2h
    ; xor dx, dx                                ; 33 d2
    int 010h                                  ; cd 10
    push 009eeh                               ; 68 ee 09
    push strict byte 00002h                   ; 6a 02
    call 018e9h                               ; e8 d3 a6
    add sp, strict byte 00004h                ; 83 c4 04
    call 06f46h                               ; e8 2a fd
    push 00a32h                               ; 68 32 0a
    push strict byte 00002h                   ; 6a 02
    call 018e9h                               ; e8 c5 a6
    add sp, strict byte 00004h                ; 83 c4 04
    mov dx, strict word 00001h                ; ba 01 00
    mov ax, strict word 00040h                ; b8 40 00
    call 06e75h                               ; e8 45 fc
    mov bl, al                                ; 88 c3
    test al, al                               ; 84 c0
    je short 07227h                           ; 74 f1
    cmp AL, strict byte 030h                  ; 3c 30
    je short 07288h                           ; 74 4e
    cmp bl, 002h                              ; 80 fb 02
    jc short 07261h                           ; 72 22
    cmp bl, 009h                              ; 80 fb 09
    jnbe short 07261h                         ; 77 1d
    movzx ax, bl                              ; 0f b6 c3
    call 07072h                               ; e8 28 fe
    cmp AL, strict byte 0ffh                  ; 3c ff
    jne short 07250h                          ; 75 02
    jmp short 07227h                          ; eb d7
    movzx bx, al                              ; 0f b6 d8
    mov dx, 00338h                            ; ba 38 03
    mov ax, si                                ; 89 f0
    call 0160eh                               ; e8 b3 a3
    mov byte [bp-004h], 002h                  ; c6 46 fc 02
    jmp short 07288h                          ; eb 27
    cmp bl, 02eh                              ; 80 fb 2e
    je short 07276h                           ; 74 10
    cmp bl, 026h                              ; 80 fb 26
    je short 0727ch                           ; 74 11
    cmp bl, 021h                              ; 80 fb 21
    jne short 07282h                          ; 75 12
    mov byte [bp-004h], 001h                  ; c6 46 fc 01
    jmp short 07288h                          ; eb 12
    mov byte [bp-004h], 003h                  ; c6 46 fc 03
    jmp short 07288h                          ; eb 0c
    mov byte [bp-004h], 004h                  ; c6 46 fc 04
    jmp short 07288h                          ; eb 06
    cmp byte [bp-004h], 000h                  ; 80 7e fc 00
    je short 07227h                           ; 74 9f
    movzx bx, byte [bp-004h]                  ; 0f b6 5e fc
    mov dx, 00339h                            ; ba 39 03
    mov ax, si                                ; 89 f0
    call 0160eh                               ; e8 7a a3
    mov AL, strict byte 003h                  ; b0 03
    mov AH, strict byte 000h                  ; b4 00
    int 010h                                  ; cd 10
    mov AL, strict byte 034h                  ; b0 34
    out strict byte 043h, AL                  ; e6 43
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    out strict byte 040h, AL                  ; e6 40
    out strict byte 040h, AL                  ; e6 40
    pushad                                    ; 66 60
    push DS                                   ; 1e
    mov ds, ax                                ; 8e d8
    call 0edbfh                               ; e8 13 7b
    pop DS                                    ; 1f
    popad                                     ; 66 61
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
delay_boot_:                                 ; 0xf72b6 LB 0x64
    push bx                                   ; 53
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, ax                                ; 89 c2
    test ax, ax                               ; 85 c0
    je short 07316h                           ; 74 55
    mov AL, strict byte 034h                  ; b0 34
    out strict byte 043h, AL                  ; e6 43
    mov AL, strict byte 0d3h                  ; b0 d3
    out strict byte 040h, AL                  ; e6 40
    mov AL, strict byte 048h                  ; b0 48
    out strict byte 040h, AL                  ; e6 40
    push dx                                   ; 52
    push 00a7ch                               ; 68 7c 0a
    push strict byte 00002h                   ; 6a 02
    call 018e9h                               ; e8 13 a6
    add sp, strict byte 00006h                ; 83 c4 06
    mov bx, dx                                ; 89 d3
    test bx, bx                               ; 85 db
    jbe short 072f6h                          ; 76 17
    push bx                                   ; 53
    push 00a9ah                               ; 68 9a 0a
    push strict byte 00002h                   ; 6a 02
    call 018e9h                               ; e8 01 a6
    add sp, strict byte 00006h                ; 83 c4 06
    xor dx, dx                                ; 31 d2
    mov ax, strict word 00040h                ; b8 40 00
    call 06e75h                               ; e8 82 fb
    dec bx                                    ; 4b
    jmp short 072dbh                          ; eb e5
    push 009cah                               ; 68 ca 09
    push strict byte 00002h                   ; 6a 02
    call 018e9h                               ; e8 eb a5
    add sp, strict byte 00004h                ; 83 c4 04
    mov AL, strict byte 034h                  ; b0 34
    out strict byte 043h, AL                  ; e6 43
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    out strict byte 040h, AL                  ; e6 40
    out strict byte 040h, AL                  ; e6 40
    pushad                                    ; 66 60
    push DS                                   ; 1e
    mov ds, ax                                ; 8e d8
    call 0edbfh                               ; e8 ac 7a
    pop DS                                    ; 1f
    popad                                     ; 66 61
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    retn                                      ; c3
scsi_cmd_data_in_:                           ; 0xf731a LB 0x88
    push si                                   ; 56
    push di                                   ; 57
    enter 00006h, 000h                        ; c8 06 00 00
    mov si, ax                                ; 89 c6
    mov byte [bp-002h], dl                    ; 88 56 fe
    mov word [bp-006h], bx                    ; 89 5e fa
    mov word [bp-004h], cx                    ; 89 4e fc
    mov dx, si                                ; 89 f2
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 0732bh                          ; 75 f7
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    mov dx, word [bp+010h]                    ; 8b 56 10
    mov cx, strict word 0000ch                ; b9 0c 00
    shr dx, 1                                 ; d1 ea
    rcr ax, 1                                 ; d1 d8
    loop 0733dh                               ; e2 fa
    and ax, 000f0h                            ; 25 f0 00
    movzx bx, byte [bp+008h]                  ; 0f b6 5e 08
    or bx, ax                                 ; 09 c3
    mov al, byte [bp-002h]                    ; 8a 46 fe
    mov dx, si                                ; 89 f2
    out DX, AL                                ; ee
    xor al, al                                ; 30 c0
    out DX, AL                                ; ee
    mov al, bl                                ; 88 d8
    out DX, AL                                ; ee
    mov al, byte [bp+00eh]                    ; 8a 46 0e
    out DX, AL                                ; ee
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    mov dx, word [bp+010h]                    ; 8b 56 10
    mov cx, strict word 00008h                ; b9 08 00
    shr dx, 1                                 ; d1 ea
    rcr ax, 1                                 ; d1 d8
    loop 07365h                               ; e2 fa
    mov dx, si                                ; 89 f2
    out DX, AL                                ; ee
    xor bx, bx                                ; 31 db
    movzx ax, byte [bp+008h]                  ; 0f b6 46 08
    cmp bx, ax                                ; 39 c3
    jnc short 07386h                          ; 73 0e
    les di, [bp-006h]                         ; c4 7e fa
    add di, bx                                ; 01 df
    mov al, byte [es:di]                      ; 26 8a 05
    mov dx, si                                ; 89 f2
    out DX, AL                                ; ee
    inc bx                                    ; 43
    jmp short 07370h                          ; eb ea
    mov dx, si                                ; 89 f2
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 07386h                          ; 75 f7
    lea dx, [si+001h]                         ; 8d 54 01
    mov cx, word [bp+00eh]                    ; 8b 4e 0e
    les di, [bp+00ah]                         ; c4 7e 0a
    rep insb                                  ; f3 6c
    xor ax, ax                                ; 31 c0
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn 0000ah                               ; c2 0a 00
scsi_cmd_data_out_:                          ; 0xf73a2 LB 0x89
    push si                                   ; 56
    push di                                   ; 57
    enter 00006h, 000h                        ; c8 06 00 00
    mov di, ax                                ; 89 c7
    mov byte [bp-002h], dl                    ; 88 56 fe
    mov word [bp-006h], bx                    ; 89 5e fa
    mov word [bp-004h], cx                    ; 89 4e fc
    mov dx, di                                ; 89 fa
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 073b3h                          ; 75 f7
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    mov dx, word [bp+010h]                    ; 8b 56 10
    mov cx, strict word 0000ch                ; b9 0c 00
    shr dx, 1                                 ; d1 ea
    rcr ax, 1                                 ; d1 d8
    loop 073c5h                               ; e2 fa
    and ax, 000f0h                            ; 25 f0 00
    movzx bx, byte [bp+008h]                  ; 0f b6 5e 08
    or bx, ax                                 ; 09 c3
    mov al, byte [bp-002h]                    ; 8a 46 fe
    mov dx, di                                ; 89 fa
    out DX, AL                                ; ee
    mov AL, strict byte 001h                  ; b0 01
    out DX, AL                                ; ee
    mov al, bl                                ; 88 d8
    out DX, AL                                ; ee
    mov al, byte [bp+00eh]                    ; 8a 46 0e
    out DX, AL                                ; ee
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    mov dx, word [bp+010h]                    ; 8b 56 10
    mov cx, strict word 00008h                ; b9 08 00
    shr dx, 1                                 ; d1 ea
    rcr ax, 1                                 ; d1 d8
    loop 073edh                               ; e2 fa
    mov dx, di                                ; 89 fa
    out DX, AL                                ; ee
    xor bx, bx                                ; 31 db
    movzx ax, byte [bp+008h]                  ; 0f b6 46 08
    cmp bx, ax                                ; 39 c3
    jnc short 0740eh                          ; 73 0e
    les si, [bp-006h]                         ; c4 76 fa
    add si, bx                                ; 01 de
    mov al, byte [es:si]                      ; 26 8a 04
    mov dx, di                                ; 89 fa
    out DX, AL                                ; ee
    inc bx                                    ; 43
    jmp short 073f8h                          ; eb ea
    lea dx, [di+001h]                         ; 8d 55 01
    mov cx, word [bp+00eh]                    ; 8b 4e 0e
    les si, [bp+00ah]                         ; c4 76 0a
    db  0f3h, 026h, 06eh
    ; rep es outsb                              ; f3 26 6e
    mov dx, di                                ; 89 fa
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 0741ah                          ; 75 f7
    xor ax, ax                                ; 31 c0
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn 0000ah                               ; c2 0a 00
@scsi_read_sectors:                          ; 0xf742b LB 0xb1
    push si                                   ; 56
    push di                                   ; 57
    enter 0000eh, 000h                        ; c8 0e 00 00
    mov si, word [bp+008h]                    ; 8b 76 08
    mov es, [bp+00ah]                         ; 8e 46 0a
    mov bl, byte [es:si+008h]                 ; 26 8a 5c 08
    sub bl, 008h                              ; 80 eb 08
    cmp bl, 004h                              ; 80 fb 04
    jbe short 07452h                          ; 76 0f
    movzx ax, bl                              ; 0f b6 c3
    push ax                                   ; 50
    push 00a9eh                               ; 68 9e 0a
    push strict byte 00007h                   ; 6a 07
    call 018e9h                               ; e8 9a a4
    add sp, strict byte 00006h                ; 83 c4 06
    mov es, [bp+00ah]                         ; 8e 46 0a
    mov di, word [es:si+00ah]                 ; 26 8b 7c 0a
    mov word [bp-00eh], strict word 00028h    ; c7 46 f2 28 00
    mov ax, word [es:si]                      ; 26 8b 04
    mov dx, word [es:si+002h]                 ; 26 8b 54 02
    xchg ah, al                               ; 86 c4
    xchg dh, dl                               ; 86 d6
    xchg dx, ax                               ; 92
    mov word [bp-00ch], ax                    ; 89 46 f4
    mov word [bp-00ah], dx                    ; 89 56 f6
    mov byte [bp-008h], 000h                  ; c6 46 f8 00
    mov ax, di                                ; 89 f8
    xchg ah, al                               ; 86 c4
    mov word [bp-007h], ax                    ; 89 46 f9
    mov byte [bp-005h], 000h                  ; c6 46 fb 00
    xor bh, bh                                ; 30 ff
    sal bx, 002h                              ; c1 e3 02
    add bx, si                                ; 01 f3
    mov ax, word [es:bx+001d8h]               ; 26 8b 87 d8 01
    mov dl, byte [es:bx+001dah]               ; 26 8a 97 da 01
    mov word [bp-004h], di                    ; 89 7e fc
    mov word [bp-002h], strict word 00000h    ; c7 46 fe 00 00
    mov cx, strict word 00009h                ; b9 09 00
    sal word [bp-004h], 1                     ; d1 66 fc
    rcl word [bp-002h], 1                     ; d1 56 fe
    loop 0749bh                               ; e2 f8
    push dword [bp-004h]                      ; 66 ff 76 fc
    db  066h, 026h, 0ffh, 074h, 004h
    ; push dword [es:si+004h]                   ; 66 26 ff 74 04
    push strict byte 0000ah                   ; 6a 0a
    xor dh, dh                                ; 30 f6
    mov cx, ss                                ; 8c d1
    lea bx, [bp-00eh]                         ; 8d 5e f2
    call 0731ah                               ; e8 62 fe
    mov ah, al                                ; 88 c4
    test al, al                               ; 84 c0
    jne short 074d3h                          ; 75 15
    mov es, [bp+00ah]                         ; 8e 46 0a
    mov word [es:si+014h], di                 ; 26 89 7c 14
    mov dx, word [bp-004h]                    ; 8b 56 fc
    mov word [es:si+016h], dx                 ; 26 89 54 16
    mov dx, word [bp-002h]                    ; 8b 56 fe
    mov word [es:si+018h], dx                 ; 26 89 54 18
    movzx ax, ah                              ; 0f b6 c4
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn 00004h                               ; c2 04 00
@scsi_write_sectors:                         ; 0xf74dc LB 0xb1
    push si                                   ; 56
    push di                                   ; 57
    enter 0000eh, 000h                        ; c8 0e 00 00
    mov si, word [bp+008h]                    ; 8b 76 08
    mov es, [bp+00ah]                         ; 8e 46 0a
    mov bl, byte [es:si+008h]                 ; 26 8a 5c 08
    sub bl, 008h                              ; 80 eb 08
    cmp bl, 004h                              ; 80 fb 04
    jbe short 07503h                          ; 76 0f
    movzx ax, bl                              ; 0f b6 c3
    push ax                                   ; 50
    push 00acch                               ; 68 cc 0a
    push strict byte 00007h                   ; 6a 07
    call 018e9h                               ; e8 e9 a3
    add sp, strict byte 00006h                ; 83 c4 06
    mov es, [bp+00ah]                         ; 8e 46 0a
    mov di, word [es:si+00ah]                 ; 26 8b 7c 0a
    mov word [bp-00eh], strict word 0002ah    ; c7 46 f2 2a 00
    mov ax, word [es:si]                      ; 26 8b 04
    mov dx, word [es:si+002h]                 ; 26 8b 54 02
    xchg ah, al                               ; 86 c4
    xchg dh, dl                               ; 86 d6
    xchg dx, ax                               ; 92
    mov word [bp-00ch], ax                    ; 89 46 f4
    mov word [bp-00ah], dx                    ; 89 56 f6
    mov byte [bp-008h], 000h                  ; c6 46 f8 00
    mov ax, di                                ; 89 f8
    xchg ah, al                               ; 86 c4
    mov word [bp-007h], ax                    ; 89 46 f9
    mov byte [bp-005h], 000h                  ; c6 46 fb 00
    xor bh, bh                                ; 30 ff
    sal bx, 002h                              ; c1 e3 02
    add bx, si                                ; 01 f3
    mov ax, word [es:bx+001d8h]               ; 26 8b 87 d8 01
    mov dl, byte [es:bx+001dah]               ; 26 8a 97 da 01
    mov word [bp-004h], di                    ; 89 7e fc
    mov word [bp-002h], strict word 00000h    ; c7 46 fe 00 00
    mov cx, strict word 00009h                ; b9 09 00
    sal word [bp-004h], 1                     ; d1 66 fc
    rcl word [bp-002h], 1                     ; d1 56 fe
    loop 0754ch                               ; e2 f8
    push dword [bp-004h]                      ; 66 ff 76 fc
    db  066h, 026h, 0ffh, 074h, 004h
    ; push dword [es:si+004h]                   ; 66 26 ff 74 04
    push strict byte 0000ah                   ; 6a 0a
    xor dh, dh                                ; 30 f6
    mov cx, ss                                ; 8c d1
    lea bx, [bp-00eh]                         ; 8d 5e f2
    call 073a2h                               ; e8 39 fe
    mov ah, al                                ; 88 c4
    test al, al                               ; 84 c0
    jne short 07584h                          ; 75 15
    mov es, [bp+00ah]                         ; 8e 46 0a
    mov word [es:si+014h], di                 ; 26 89 7c 14
    mov dx, word [bp-004h]                    ; 8b 56 fc
    mov word [es:si+016h], dx                 ; 26 89 54 16
    mov dx, word [bp-002h]                    ; 8b 56 fe
    mov word [es:si+018h], dx                 ; 26 89 54 18
    movzx ax, ah                              ; 0f b6 c4
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn 00004h                               ; c2 04 00
scsi_cmd_packet_:                            ; 0xf758d LB 0x161
    push si                                   ; 56
    push di                                   ; 57
    enter 0000ch, 000h                        ; c8 0c 00 00
    mov di, ax                                ; 89 c7
    mov byte [bp-002h], dl                    ; 88 56 fe
    mov word [bp-008h], bx                    ; 89 5e f8
    mov word [bp-006h], cx                    ; 89 4e fa
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 75 a0
    mov si, 00122h                            ; be 22 01
    mov word [bp-00ah], ax                    ; 89 46 f6
    cmp byte [bp+00eh], 002h                  ; 80 7e 0e 02
    jne short 075d2h                          ; 75 1f
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 f0 a2
    push 00afbh                               ; 68 fb 0a
    push 00b0bh                               ; 68 0b 0b
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 20 a3
    add sp, strict byte 00006h                ; 83 c4 06
    mov dx, strict word 00001h                ; ba 01 00
    jmp near 076e6h                           ; e9 14 01
    sub di, strict byte 00008h                ; 83 ef 08
    sal di, 002h                              ; c1 e7 02
    sub byte [bp-002h], 002h                  ; 80 6e fe 02
    mov es, [bp-00ah]                         ; 8e 46 f6
    add di, si                                ; 01 f7
    mov bx, word [es:di+001d8h]               ; 26 8b 9d d8 01
    mov al, byte [es:di+001dah]               ; 26 8a 85 da 01
    mov byte [bp-004h], al                    ; 88 46 fc
    mov dx, bx                                ; 89 da
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 075eeh                          ; 75 f7
    xor ax, ax                                ; 31 c0
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, word [bp+008h]                    ; 03 56 08
    adc ax, word [bp+00ch]                    ; 13 46 0c
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov cx, word [es:si+01ch]                 ; 26 8b 4c 1c
    xor di, di                                ; 31 ff
    add cx, dx                                ; 01 d1
    mov word [bp-00ch], cx                    ; 89 4e f4
    adc di, ax                                ; 11 c7
    mov ax, cx                                ; 89 c8
    mov dx, di                                ; 89 fa
    mov cx, strict word 0000ch                ; b9 0c 00
    shr dx, 1                                 ; d1 ea
    rcr ax, 1                                 ; d1 d8
    loop 07619h                               ; e2 fa
    and ax, 000f0h                            ; 25 f0 00
    movzx cx, byte [bp-002h]                  ; 0f b6 4e fe
    or cx, ax                                 ; 09 c1
    mov al, byte [bp-004h]                    ; 8a 46 fc
    mov dx, bx                                ; 89 da
    out DX, AL                                ; ee
    xor al, al                                ; 30 c0
    out DX, AL                                ; ee
    mov al, cl                                ; 88 c8
    out DX, AL                                ; ee
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    out DX, AL                                ; ee
    mov ax, word [bp-00ch]                    ; 8b 46 f4
    mov dx, di                                ; 89 fa
    mov cx, strict word 00008h                ; b9 08 00
    shr dx, 1                                 ; d1 ea
    rcr ax, 1                                 ; d1 d8
    loop 07640h                               ; e2 fa
    mov dx, bx                                ; 89 da
    out DX, AL                                ; ee
    xor cx, cx                                ; 31 c9
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    cmp cx, ax                                ; 39 c1
    jnc short 07661h                          ; 73 0e
    les di, [bp-008h]                         ; c4 7e f8
    add di, cx                                ; 01 cf
    mov al, byte [es:di]                      ; 26 8a 05
    mov dx, bx                                ; 89 da
    out DX, AL                                ; ee
    inc cx                                    ; 41
    jmp short 0764bh                          ; eb ea
    mov dx, bx                                ; 89 da
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 07661h                          ; 75 f7
    test AL, strict byte 002h                 ; a8 02
    je short 0767ch                           ; 74 0e
    lea dx, [bx+003h]                         ; 8d 57 03
    xor al, al                                ; 30 c0
    out DX, AL                                ; ee
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov dx, strict word 00003h                ; ba 03 00
    jmp short 076e6h                          ; eb 6a
    mov ax, word [bp+008h]                    ; 8b 46 08
    test ax, ax                               ; 85 c0
    je short 0768bh                           ; 74 08
    lea dx, [bx+001h]                         ; 8d 57 01
    mov cx, ax                                ; 89 c1
    in AL, DX                                 ; ec
    loop 07688h                               ; e2 fd
    mov ax, word [bp+00ah]                    ; 8b 46 0a
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov word [es:si+016h], ax                 ; 26 89 44 16
    mov ax, word [bp+00ch]                    ; 8b 46 0c
    mov word [es:si+018h], ax                 ; 26 89 44 18
    lea ax, [bx+001h]                         ; 8d 47 01
    cmp word [bp+00ch], strict byte 00000h    ; 83 7e 0c 00
    jne short 076ach                          ; 75 07
    cmp word [bp+00ah], 08000h                ; 81 7e 0a 00 80
    jbe short 076c9h                          ; 76 1d
    mov dx, ax                                ; 89 c2
    mov cx, 08000h                            ; b9 00 80
    les di, [bp+010h]                         ; c4 7e 10
    rep insb                                  ; f3 6c
    add word [bp+00ah], 08000h                ; 81 46 0a 00 80
    adc word [bp+00ch], strict byte 0ffffh    ; 83 56 0c ff
    mov ax, es                                ; 8c c0
    add ax, 00800h                            ; 05 00 08
    mov word [bp+012h], ax                    ; 89 46 12
    jmp short 0769ch                          ; eb d3
    mov dx, ax                                ; 89 c2
    mov cx, word [bp+00ah]                    ; 8b 4e 0a
    les di, [bp+010h]                         ; c4 7e 10
    rep insb                                  ; f3 6c
    mov es, [bp-00ah]                         ; 8e 46 f6
    cmp word [es:si+01ch], strict byte 00000h ; 26 83 7c 1c 00
    je short 076e4h                           ; 74 07
    mov cx, word [es:si+01ch]                 ; 26 8b 4c 1c
    in AL, DX                                 ; ec
    loop 076e1h                               ; e2 fd
    xor dx, dx                                ; 31 d2
    mov ax, dx                                ; 89 d0
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn 0000ch                               ; c2 0c 00
scsi_enumerate_attached_devices_:            ; 0xf76ee LB 0x36d
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push si                                   ; 56
    push di                                   ; 57
    enter 0021eh, 000h                        ; c8 1e 02 00
    push ax                                   ; 50
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 1b 9f
    mov si, 00122h                            ; be 22 01
    mov word [bp-008h], ax                    ; 89 46 f8
    mov word [bp-00ah], strict word 00000h    ; c7 46 f6 00 00
    jmp near 079ech                           ; e9 dd 02
    mov es, [bp-008h]                         ; 8e 46 f8
    cmp byte [es:si+001e8h], 004h             ; 26 80 bc e8 01 04
    jnc near 07a54h                           ; 0f 83 38 03
    mov cx, strict word 0000ah                ; b9 0a 00
    xor bx, bx                                ; 31 db
    mov dx, ss                                ; 8c d2
    lea ax, [bp-01eh]                         ; 8d 46 e2
    call 0901ah                               ; e8 f1 18
    mov byte [bp-01eh], 025h                  ; c6 46 e2 25
    push dword 000000008h                     ; 66 6a 08
    lea dx, [bp-0021eh]                       ; 8d 96 e2 fd
    push SS                                   ; 16
    push dx                                   ; 52
    push strict byte 0000ah                   ; 6a 0a
    movzx dx, byte [bp-00ah]                  ; 0f b6 56 f6
    mov cx, ss                                ; 8c d1
    lea bx, [bp-01eh]                         ; 8d 5e e2
    mov ax, word [bp-00220h]                  ; 8b 86 e0 fd
    call 0731ah                               ; e8 d2 fb
    test al, al                               ; 84 c0
    je short 0775ah                           ; 74 0e
    push 00b2bh                               ; 68 2b 0b
    push 00b64h                               ; 68 64 0b
    push strict byte 00007h                   ; 6a 07
    call 018e9h                               ; e8 92 a1
    add sp, strict byte 00006h                ; 83 c4 06
    movzx dx, byte [bp-0021dh]                ; 0f b6 96 e3 fd
    movzx di, byte [bp-0021eh]                ; 0f b6 be e2 fd
    sal di, 008h                              ; c1 e7 08
    xor bx, bx                                ; 31 db
    or di, dx                                 ; 09 d7
    movzx ax, byte [bp-0021ch]                ; 0f b6 86 e4 fd
    xor dx, dx                                ; 31 d2
    mov cx, strict word 00008h                ; b9 08 00
    sal ax, 1                                 ; d1 e0
    rcl dx, 1                                 ; d1 d2
    loop 07775h                               ; e2 fa
    or ax, bx                                 ; 09 d8
    or di, dx                                 ; 09 d7
    movzx dx, byte [bp-0021bh]                ; 0f b6 96 e5 fd
    or ax, dx                                 ; 09 d0
    mov word [bp-006h], ax                    ; 89 46 fa
    movzx dx, byte [bp-0021ah]                ; 0f b6 96 e6 fd
    sal dx, 008h                              ; c1 e2 08
    movzx ax, byte [bp-00219h]                ; 0f b6 86 e7 fd
    or dx, ax                                 ; 09 c2
    mov word [bp-014h], dx                    ; 89 56 ec
    movzx ax, byte [bp-00218h]                ; 0f b6 86 e8 fd
    xor dx, dx                                ; 31 d2
    mov cx, strict word 00008h                ; b9 08 00
    sal ax, 1                                 ; d1 e0
    rcl dx, 1                                 ; d1 d2
    loop 077a5h                               ; e2 fa
    or bx, ax                                 ; 09 c3
    or dx, word [bp-014h]                     ; 0b 56 ec
    movzx ax, byte [bp-00217h]                ; 0f b6 86 e9 fd
    or bx, ax                                 ; 09 c3
    mov word [bp-004h], bx                    ; 89 5e fc
    test dx, dx                               ; 85 d2
    jne short 077c4h                          ; 75 06
    cmp bx, 00200h                            ; 81 fb 00 02
    je short 077e4h                           ; 74 20
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 df a0
    push dx                                   ; 52
    push word [bp-004h]                       ; ff 76 fc
    push word [bp-00ah]                       ; ff 76 f6
    push 00b83h                               ; 68 83 0b
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 0b a1
    add sp, strict byte 0000ah                ; 83 c4 0a
    jmp near 079e3h                           ; e9 ff 01
    cmp di, strict byte 00040h                ; 83 ff 40
    jnbe short 077ebh                         ; 77 02
    jne short 077f5h                          ; 75 0a
    mov dword [bp-00eh], strict dword 000ff003fh ; 66 c7 46 f2 3f 00 ff 00
    jmp short 0780eh                          ; eb 19
    cmp di, strict byte 00020h                ; 83 ff 20
    jnbe short 077fch                         ; 77 02
    jne short 07806h                          ; 75 0a
    mov dword [bp-00eh], strict dword 000800020h ; 66 c7 46 f2 20 00 80 00
    jmp short 0780eh                          ; eb 08
    mov dword [bp-00eh], strict dword 000400020h ; 66 c7 46 f2 20 00 40 00
    mov bx, word [bp-00ch]                    ; 8b 5e f4
    imul bx, word [bp-00eh]                   ; 0f af 5e f2
    mov ax, word [bp-006h]                    ; 8b 46 fa
    mov dx, di                                ; 89 fa
    xor cx, cx                                ; 31 c9
    call 08fb0h                               ; e8 91 17
    mov word [bp-012h], ax                    ; 89 46 ee
    mov word [bp-010h], dx                    ; 89 56 f0
    mov es, [bp-008h]                         ; 8e 46 f8
    mov al, byte [es:si+001e8h]               ; 26 8a 84 e8 01
    mov byte [bp-002h], al                    ; 88 46 fe
    mov dl, al                                ; 88 c2
    add dl, 008h                              ; 80 c2 08
    movzx bx, al                              ; 0f b6 d8
    sal bx, 002h                              ; c1 e3 02
    add bx, si                                ; 01 f3
    mov ax, word [bp-00220h]                  ; 8b 86 e0 fd
    mov word [es:bx+001d8h], ax               ; 26 89 87 d8 01
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    mov byte [es:bx+001dah], al               ; 26 88 87 da 01
    movzx ax, dl                              ; 0f b6 c2
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    db  066h, 026h, 0c7h, 047h, 01eh, 004h, 0ffh, 000h, 000h
    ; mov dword [es:bx+01eh], strict dword 00000ff04h ; 66 26 c7 47 1e 04 ff 00 00
    mov ax, word [bp-004h]                    ; 8b 46 fc
    mov word [es:bx+024h], ax                 ; 26 89 47 24
    mov byte [es:bx+023h], 001h               ; 26 c6 47 23 01
    mov ax, word [bp-00ch]                    ; 8b 46 f4
    mov word [es:bx+026h], ax                 ; 26 89 47 26
    mov ax, word [bp-00eh]                    ; 8b 46 f2
    mov word [es:bx+02ah], ax                 ; 26 89 47 2a
    cmp word [bp-010h], strict byte 00000h    ; 83 7e f0 00
    jne short 07888h                          ; 75 07
    cmp word [bp-012h], 00400h                ; 81 7e ee 00 04
    jbe short 07890h                          ; 76 08
    mov word [es:bx+028h], 00400h             ; 26 c7 47 28 00 04
    jmp short 07897h                          ; eb 07
    mov ax, word [bp-012h]                    ; 8b 46 ee
    mov word [es:bx+028h], ax                 ; 26 89 47 28
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 0c a0
    push di                                   ; 57
    push word [bp-006h]                       ; ff 76 fa
    push word [bp-00eh]                       ; ff 76 f2
    push word [bp-00ch]                       ; ff 76 f4
    push word [bp-012h]                       ; ff 76 ee
    push word [bp-00ah]                       ; ff 76 f6
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    push ax                                   ; 50
    push 00bb1h                               ; 68 b1 0b
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 2a a0
    add sp, strict byte 00012h                ; 83 c4 12
    movzx ax, dl                              ; 0f b6 c2
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    mov es, [bp-008h]                         ; 8e 46 f8
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    mov ax, word [bp-00ch]                    ; 8b 46 f4
    mov word [es:bx+02ch], ax                 ; 26 89 47 2c
    mov ax, word [bp-00eh]                    ; 8b 46 f2
    mov word [es:bx+030h], ax                 ; 26 89 47 30
    cmp word [bp-010h], strict byte 00000h    ; 83 7e f0 00
    jne short 078eah                          ; 75 07
    cmp word [bp-012h], 00400h                ; 81 7e ee 00 04
    jbe short 078f2h                          ; 76 08
    mov word [es:bx+02eh], 00400h             ; 26 c7 47 2e 00 04
    jmp short 078f9h                          ; eb 07
    mov ax, word [bp-012h]                    ; 8b 46 ee
    mov word [es:bx+02eh], ax                 ; 26 89 47 2e
    movzx ax, dl                              ; 0f b6 c2
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    mov es, [bp-008h]                         ; 8e 46 f8
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    mov ax, word [bp-006h]                    ; 8b 46 fa
    mov word [es:bx+032h], ax                 ; 26 89 47 32
    mov word [es:bx+034h], di                 ; 26 89 7f 34
    mov al, byte [es:si+0019eh]               ; 26 8a 84 9e 01
    mov ah, byte [bp-002h]                    ; 8a 66 fe
    add ah, 008h                              ; 80 c4 08
    movzx bx, al                              ; 0f b6 d8
    add bx, si                                ; 01 f3
    mov byte [es:bx+0019fh], ah               ; 26 88 a7 9f 01
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    mov byte [es:si+0019eh], al               ; 26 88 84 9e 01
    mov dx, strict word 00075h                ; ba 75 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 ca 9c
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 00075h                ; ba 75 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 ca 9c
    inc byte [bp-002h]                        ; fe 46 fe
    mov al, byte [bp-002h]                    ; 8a 46 fe
    mov es, [bp-008h]                         ; 8e 46 f8
    mov byte [es:si+001e8h], al               ; 26 88 84 e8 01
    jmp near 079e3h                           ; e9 8e 00
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 4e 9f
    push word [bp-00ah]                       ; ff 76 f6
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    push ax                                   ; 50
    push 00bdbh                               ; 68 db 0b
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 79 9f
    add sp, strict byte 00008h                ; 83 c4 08
    mov dl, byte [bp-002h]                    ; 8a 56 fe
    add dl, 008h                              ; 80 c2 08
    test byte [bp-0021dh], 080h               ; f6 86 e3 fd 80
    db  00fh, 095h, 0c0h
    ; setne al                                  ; 0f 95 c0
    xor ah, ah                                ; 30 e4
    mov cx, ax                                ; 89 c1
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    sal ax, 002h                              ; c1 e0 02
    mov es, [bp-008h]                         ; 8e 46 f8
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    mov ax, word [bp-00220h]                  ; 8b 86 e0 fd
    mov word [es:bx+001d8h], ax               ; 26 89 87 d8 01
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    mov byte [es:bx+001dah], al               ; 26 88 87 da 01
    movzx bx, dl                              ; 0f b6 da
    imul bx, bx, strict byte 00018h           ; 6b db 18
    add bx, si                                ; 01 f3
    mov word [es:bx+01eh], 00504h             ; 26 c7 47 1e 04 05
    mov byte [es:bx+020h], cl                 ; 26 88 4f 20
    mov word [es:bx+024h], 00800h             ; 26 c7 47 24 00 08
    mov al, byte [es:si+001afh]               ; 26 8a 84 af 01
    mov ah, byte [bp-002h]                    ; 8a 66 fe
    add ah, 008h                              ; 80 c4 08
    movzx bx, al                              ; 0f b6 d8
    add bx, si                                ; 01 f3
    mov byte [es:bx+001b0h], ah               ; 26 88 a7 b0 01
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    mov byte [es:si+001afh], al               ; 26 88 84 af 01
    inc byte [bp-002h]                        ; fe 46 fe
    mov al, byte [bp-002h]                    ; 8a 46 fe
    mov byte [es:si+001e8h], al               ; 26 88 84 e8 01
    inc word [bp-00ah]                        ; ff 46 f6
    cmp word [bp-00ah], strict byte 00010h    ; 83 7e f6 10
    jnl short 07a54h                          ; 7d 68
    mov byte [bp-01eh], 012h                  ; c6 46 e2 12
    xor al, al                                ; 30 c0
    mov byte [bp-01dh], al                    ; 88 46 e3
    mov byte [bp-01ch], al                    ; 88 46 e4
    mov byte [bp-01bh], al                    ; 88 46 e5
    mov byte [bp-01ah], 005h                  ; c6 46 e6 05
    mov byte [bp-019h], al                    ; 88 46 e7
    push dword 000000005h                     ; 66 6a 05
    lea dx, [bp-0021eh]                       ; 8d 96 e2 fd
    push SS                                   ; 16
    push dx                                   ; 52
    push strict byte 00006h                   ; 6a 06
    movzx dx, byte [bp-00ah]                  ; 0f b6 56 f6
    mov cx, ss                                ; 8c d1
    lea bx, [bp-01eh]                         ; 8d 5e e2
    mov ax, word [bp-00220h]                  ; 8b 86 e0 fd
    call 0731ah                               ; e8 fd f8
    test al, al                               ; 84 c0
    je short 07a2fh                           ; 74 0e
    push 00b2bh                               ; 68 2b 0b
    push 00b4bh                               ; 68 4b 0b
    push strict byte 00007h                   ; 6a 07
    call 018e9h                               ; e8 bd 9e
    add sp, strict byte 00006h                ; 83 c4 06
    test byte [bp-0021eh], 0e0h               ; f6 86 e2 fd e0
    jne short 07a3fh                          ; 75 09
    test byte [bp-0021eh], 01fh               ; f6 86 e2 fd 1f
    je near 0770fh                            ; 0f 84 d0 fc
    test byte [bp-0021eh], 0e0h               ; f6 86 e2 fd e0
    jne short 079e3h                          ; 75 9d
    mov al, byte [bp-0021eh]                  ; 8a 86 e2 fd
    and AL, strict byte 01fh                  ; 24 1f
    cmp AL, strict byte 005h                  ; 3c 05
    je near 07955h                            ; 0f 84 03 ff
    jmp short 079e3h                          ; eb 8f
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
_scsi_init:                                  ; 0xf7a5b LB 0x64
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 b5 9b
    mov bx, 00122h                            ; bb 22 01
    mov es, ax                                ; 8e c0
    mov byte [es:bx+001e8h], 000h             ; 26 c6 87 e8 01 00
    mov AL, strict byte 055h                  ; b0 55
    mov dx, 00432h                            ; ba 32 04
    out DX, AL                                ; ee
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp AL, strict byte 055h                  ; 3c 55
    jne short 07a8bh                          ; 75 0c
    xor al, al                                ; 30 c0
    mov dx, 00433h                            ; ba 33 04
    out DX, AL                                ; ee
    mov ax, 00430h                            ; b8 30 04
    call 076eeh                               ; e8 63 fc
    mov AL, strict byte 055h                  ; b0 55
    mov dx, 00436h                            ; ba 36 04
    out DX, AL                                ; ee
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp AL, strict byte 055h                  ; 3c 55
    jne short 07aa4h                          ; 75 0c
    xor al, al                                ; 30 c0
    mov dx, 00437h                            ; ba 37 04
    out DX, AL                                ; ee
    mov ax, 00434h                            ; b8 34 04
    call 076eeh                               ; e8 4a fc
    mov AL, strict byte 055h                  ; b0 55
    mov dx, 0043ah                            ; ba 3a 04
    out DX, AL                                ; ee
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp AL, strict byte 055h                  ; 3c 55
    jne short 07abdh                          ; 75 0c
    xor al, al                                ; 30 c0
    mov dx, 0043bh                            ; ba 3b 04
    out DX, AL                                ; ee
    mov ax, 00438h                            ; b8 38 04
    call 076eeh                               ; e8 31 fc
    pop bp                                    ; 5d
    retn                                      ; c3
high_bits_save_:                             ; 0xf7abf LB 0x14
    push bx                                   ; 53
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, ax                                ; 89 c3
    shr eax, 010h                             ; 66 c1 e8 10
    mov es, dx                                ; 8e c2
    mov word [es:bx+00268h], ax               ; 26 89 87 68 02
    pop bp                                    ; 5d
    pop bx                                    ; 5b
    retn                                      ; c3
high_bits_restore_:                          ; 0xf7ad3 LB 0x14
    push bx                                   ; 53
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, ax                                ; 89 c3
    mov es, dx                                ; 8e c2
    mov ax, word [es:bx+00268h]               ; 26 8b 87 68 02
    sal eax, 010h                             ; 66 c1 e0 10
    pop bp                                    ; 5d
    pop bx                                    ; 5b
    retn                                      ; c3
ahci_ctrl_set_bits_:                         ; 0xf7ae7 LB 0x40
    push si                                   ; 56
    push di                                   ; 57
    enter 00002h, 000h                        ; c8 02 00 00
    mov si, ax                                ; 89 c6
    mov ax, dx                                ; 89 d0
    mov word [bp-002h], bx                    ; 89 5e fe
    mov di, cx                                ; 89 cf
    xor cx, cx                                ; 31 c9
    mov dx, si                                ; 89 f2
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    lea bx, [si+004h]                         ; 8d 5c 04
    mov dx, bx                                ; 89 da
    in eax, DX                                ; 66 ed
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0
    shr eax, 010h                             ; 66 c1 e8 10
    xchg dx, ax                               ; 92
    or ax, word [bp-002h]                     ; 0b 46 fe
    mov cx, dx                                ; 89 d1
    or cx, di                                 ; 09 f9
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
ahci_ctrl_clear_bits_:                       ; 0xf7b27 LB 0x44
    push si                                   ; 56
    push di                                   ; 57
    enter 00002h, 000h                        ; c8 02 00 00
    mov si, ax                                ; 89 c6
    mov ax, dx                                ; 89 d0
    mov di, bx                                ; 89 df
    mov word [bp-002h], cx                    ; 89 4e fe
    xor cx, cx                                ; 31 c9
    mov dx, si                                ; 89 f2
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    lea bx, [si+004h]                         ; 8d 5c 04
    mov dx, bx                                ; 89 da
    in eax, DX                                ; 66 ed
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0
    shr eax, 010h                             ; 66 c1 e8 10
    xchg dx, ax                               ; 92
    not di                                    ; f7 d7
    mov cx, word [bp-002h]                    ; 8b 4e fe
    not cx                                    ; f7 d1
    and ax, di                                ; 21 f8
    and cx, dx                                ; 21 d1
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
ahci_ctrl_is_bit_set_:                       ; 0xf7b6b LB 0x36
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov si, ax                                ; 89 c6
    mov ax, dx                                ; 89 d0
    mov di, cx                                ; 89 cf
    xor cx, cx                                ; 31 c9
    mov dx, si                                ; 89 f2
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    lea dx, [si+004h]                         ; 8d 54 04
    in eax, DX                                ; 66 ed
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0
    shr eax, 010h                             ; 66 c1 e8 10
    xchg dx, ax                               ; 92
    test dx, di                               ; 85 fa
    jne short 07b97h                          ; 75 04
    test ax, bx                               ; 85 d8
    je short 07b9bh                           ; 74 04
    mov AL, strict byte 001h                  ; b0 01
    jmp short 07b9dh                          ; eb 02
    xor al, al                                ; 30 c0
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
ahci_ctrl_extract_bits_:                     ; 0xf7ba1 LB 0x1b
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov si, bx                                ; 89 de
    and ax, bx                                ; 21 d8
    and dx, cx                                ; 21 ca
    movzx cx, byte [bp+006h]                  ; 0f b6 4e 06
    jcxz 07bb7h                               ; e3 06
    shr dx, 1                                 ; d1 ea
    rcr ax, 1                                 ; d1 d8
    loop 07bb1h                               ; e2 fa
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn 00002h                               ; c2 02 00
ahci_addr_to_phys_:                          ; 0xf7bbc LB 0x1e
    push bx                                   ; 53
    push cx                                   ; 51
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, ax                                ; 89 c3
    mov ax, dx                                ; 89 d0
    xor dx, dx                                ; 31 d2
    mov cx, strict word 00004h                ; b9 04 00
    sal ax, 1                                 ; d1 e0
    rcl dx, 1                                 ; d1 d2
    loop 07bcah                               ; e2 fa
    xor cx, cx                                ; 31 c9
    add ax, bx                                ; 01 d8
    adc dx, cx                                ; 11 ca
    pop bp                                    ; 5d
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
ahci_port_cmd_sync_:                         ; 0xf7bda LB 0xd0
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    enter 00006h, 000h                        ; c8 06 00 00
    mov si, ax                                ; 89 c6
    mov word [bp-004h], dx                    ; 89 56 fc
    mov es, dx                                ; 8e c2
    mov al, byte [es:si+00262h]               ; 26 8a 84 62 02
    mov byte [bp-002h], al                    ; 88 46 fe
    mov di, word [es:si+00260h]               ; 26 8b bc 60 02
    cmp AL, strict byte 0ffh                  ; 3c ff
    je near 07ca5h                            ; 0f 84 aa 00
    movzx cx, byte [es:si+00263h]             ; 26 0f b6 8c 63 02
    xor dx, dx                                ; 31 d2
    or dl, 080h                               ; 80 ca 80
    movzx ax, bl                              ; 0f b6 c3
    or dx, ax                                 ; 09 c2
    mov word [es:si], dx                      ; 26 89 14
    mov word [es:si+002h], cx                 ; 26 89 4c 02
    db  066h, 026h, 0c7h, 044h, 004h, 000h, 000h, 000h, 000h
    ; mov dword [es:si+004h], strict dword 000000000h ; 66 26 c7 44 04 00 00 00 00
    lea ax, [si+00080h]                       ; 8d 84 80 00
    mov dx, es                                ; 8c c2
    call 07bbch                               ; e8 98 ff
    mov es, [bp-004h]                         ; 8e 46 fc
    mov word [es:si+008h], ax                 ; 26 89 44 08
    mov word [es:si+00ah], dx                 ; 26 89 54 0a
    movzx si, byte [bp-002h]                  ; 0f b6 76 fe
    sal si, 007h                              ; c1 e6 07
    lea dx, [si+00118h]                       ; 8d 94 18 01
    mov bx, strict word 00011h                ; bb 11 00
    xor cx, cx                                ; 31 c9
    mov ax, di                                ; 89 f8
    call 07ae7h                               ; e8 a3 fe
    lea ax, [si+00138h]                       ; 8d 84 38 01
    cwd                                       ; 99
    mov cx, dx                                ; 89 d1
    mov dx, di                                ; 89 fa
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    lea dx, [di+004h]                         ; 8d 55 04
    mov ax, strict word 00001h                ; b8 01 00
    xor cx, cx                                ; 31 c9
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    sal ax, 007h                              ; c1 e0 07
    mov word [bp-006h], ax                    ; 89 46 fa
    mov si, ax                                ; 89 c6
    add si, 00110h                            ; 81 c6 10 01
    mov bx, strict word 00001h                ; bb 01 00
    mov cx, 04000h                            ; b9 00 40
    mov dx, si                                ; 89 f2
    mov ax, di                                ; 89 f8
    call 07b6bh                               ; e8 e7 fe
    test al, al                               ; 84 c0
    je short 07c67h                           ; 74 df
    mov bx, strict word 00001h                ; bb 01 00
    xor cx, cx                                ; 31 c9
    mov dx, si                                ; 89 f2
    mov ax, di                                ; 89 f8
    call 07ae7h                               ; e8 53 fe
    mov dx, word [bp-006h]                    ; 8b 56 fa
    add dx, 00118h                            ; 81 c2 18 01
    mov bx, strict word 00001h                ; bb 01 00
    xor cx, cx                                ; 31 c9
    mov ax, di                                ; 89 f8
    call 07b27h                               ; e8 82 fe
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    retn                                      ; c3
ahci_cmd_data_:                              ; 0xf7caa LB 0x1c5
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    enter 00010h, 000h                        ; c8 10 00 00
    mov di, ax                                ; 89 c7
    mov word [bp-00ch], dx                    ; 89 56 f4
    mov byte [bp-002h], bl                    ; 88 5e fe
    xor si, si                                ; 31 f6
    mov es, dx                                ; 8e c2
    mov ax, word [es:di+001eeh]               ; 26 8b 85 ee 01
    mov word [bp-004h], ax                    ; 89 46 fc
    mov word [bp-008h], si                    ; 89 76 f8
    mov word [bp-006h], ax                    ; 89 46 fa
    mov ax, word [es:di+00ah]                 ; 26 8b 45 0a
    mov word [bp-00ah], ax                    ; 89 46 f6
    mov ax, word [es:di+00ch]                 ; 26 8b 45 0c
    mov word [bp-010h], ax                    ; 89 46 f0
    mov cx, strict word 00040h                ; b9 40 00
    xor bx, bx                                ; 31 db
    mov ax, 00080h                            ; b8 80 00
    mov dx, word [bp-004h]                    ; 8b 56 fc
    call 0901ah                               ; e8 33 13
    mov es, [bp-004h]                         ; 8e 46 fc
    mov word [es:si+00080h], 08027h           ; 26 c7 84 80 00 27 80
    mov al, byte [bp-002h]                    ; 8a 46 fe
    mov byte [es:si+00082h], al               ; 26 88 84 82 00
    mov byte [es:si+00083h], 000h             ; 26 c6 84 83 00 00
    mov es, [bp-00ch]                         ; 8e 46 f4
    mov al, byte [es:di]                      ; 26 8a 05
    mov es, [bp-004h]                         ; 8e 46 fc
    mov byte [es:si+00084h], al               ; 26 88 84 84 00
    mov es, [bp-00ch]                         ; 8e 46 f4
    mov ax, word [es:di]                      ; 26 8b 05
    mov bx, word [es:di+002h]                 ; 26 8b 5d 02
    mov cx, strict word 00008h                ; b9 08 00
    shr bx, 1                                 ; d1 eb
    rcr ax, 1                                 ; d1 d8
    loop 07d1ah                               ; e2 fa
    mov es, [bp-004h]                         ; 8e 46 fc
    mov byte [es:si+00085h], al               ; 26 88 84 85 00
    mov es, [bp-00ch]                         ; 8e 46 f4
    mov ax, word [es:di+002h]                 ; 26 8b 45 02
    mov es, [bp-004h]                         ; 8e 46 fc
    mov byte [es:si+00086h], al               ; 26 88 84 86 00
    mov byte [es:si+00087h], 040h             ; 26 c6 84 87 00 40
    mov es, [bp-00ch]                         ; 8e 46 f4
    mov ax, word [es:di+002h]                 ; 26 8b 45 02
    shr ax, 008h                              ; c1 e8 08
    mov es, [bp-004h]                         ; 8e 46 fc
    mov byte [es:si+00088h], al               ; 26 88 84 88 00
    mov word [es:si+00089h], strict word 00000h ; 26 c7 84 89 00 00 00
    mov byte [es:si+0008bh], 000h             ; 26 c6 84 8b 00 00
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    mov byte [es:si+0008ch], al               ; 26 88 84 8c 00
    mov ax, word [bp-00ah]                    ; 8b 46 f6
    shr ax, 008h                              ; c1 e8 08
    mov byte [es:si+0008dh], al               ; 26 88 84 8d 00
    mov word [es:si+00276h], strict word 00010h ; 26 c7 84 76 02 10 00
    mov ax, word [bp-00ah]                    ; 8b 46 f6
    xor dx, dx                                ; 31 d2
    mov bx, word [bp-010h]                    ; 8b 5e f0
    xor cx, cx                                ; 31 c9
    call 08fe9h                               ; e8 66 12
    push dx                                   ; 52
    push ax                                   ; 50
    mov es, [bp-00ch]                         ; 8e 46 f4
    mov bx, word [es:di+004h]                 ; 26 8b 5d 04
    mov cx, word [es:di+006h]                 ; 26 8b 4d 06
    mov ax, 0026ah                            ; b8 6a 02
    mov dx, word [bp-004h]                    ; 8b 56 fc
    call 08effh                               ; e8 66 11
    mov es, [bp-004h]                         ; 8e 46 fc
    movzx ax, byte [es:si+00263h]             ; 26 0f b6 84 63 02
    mov dx, word [es:si+0027eh]               ; 26 8b 94 7e 02
    add dx, strict byte 0ffffh                ; 83 c2 ff
    mov bx, word [es:si+00280h]               ; 26 8b 9c 80 02
    adc bx, strict byte 0ffffh                ; 83 d3 ff
    mov word [bp-00eh], bx                    ; 89 5e f2
    mov bx, ax                                ; 89 c3
    sal bx, 004h                              ; c1 e3 04
    mov word [es:bx+0010ch], dx               ; 26 89 97 0c 01
    mov dx, word [bp-00eh]                    ; 8b 56 f2
    mov word [es:bx+0010eh], dx               ; 26 89 97 0e 01
    mov cx, word [es:si+0027ah]               ; 26 8b 8c 7a 02
    mov dx, word [es:si+0027ch]               ; 26 8b 94 7c 02
    mov word [es:bx+00100h], cx               ; 26 89 8f 00 01
    mov word [es:bx+00102h], dx               ; 26 89 97 02 01
    inc ax                                    ; 40
    mov es, [bp-00ch]                         ; 8e 46 f4
    cmp word [es:di+01ch], strict byte 00000h ; 26 83 7d 1c 00
    je short 07e12h                           ; 74 2c
    mov dx, word [es:di+01ch]                 ; 26 8b 55 1c
    dec dx                                    ; 4a
    mov di, ax                                ; 89 c7
    sal di, 004h                              ; c1 e7 04
    mov es, [bp-004h]                         ; 8e 46 fc
    mov word [es:di+0010ch], dx               ; 26 89 95 0c 01
    mov word [es:di+0010eh], si               ; 26 89 b5 0e 01
    mov dx, word [es:si+00264h]               ; 26 8b 94 64 02
    mov bx, word [es:si+00266h]               ; 26 8b 9c 66 02
    mov word [es:di+00100h], dx               ; 26 89 95 00 01
    mov word [es:di+00102h], bx               ; 26 89 9d 02 01
    inc ax                                    ; 40
    les bx, [bp-008h]                         ; c4 5e f8
    mov byte [es:bx+00263h], al               ; 26 88 87 63 02
    xor ax, ax                                ; 31 c0
    les bx, [bp-008h]                         ; c4 5e f8
    movzx dx, byte [es:bx+00263h]             ; 26 0f b6 97 63 02
    cmp ax, dx                                ; 39 d0
    jnc short 07e2ch                          ; 73 03
    inc ax                                    ; 40
    jmp short 07e1ch                          ; eb f0
    mov al, byte [bp-002h]                    ; 8a 46 fe
    cmp AL, strict byte 035h                  ; 3c 35
    jne short 07e39h                          ; 75 06
    mov byte [bp-002h], 040h                  ; c6 46 fe 40
    jmp short 07e4dh                          ; eb 14
    cmp AL, strict byte 0a0h                  ; 3c a0
    jne short 07e49h                          ; 75 0c
    or byte [bp-002h], 020h                   ; 80 4e fe 20
    or byte [es:bx+00083h], 001h              ; 26 80 8f 83 00 01
    jmp short 07e4dh                          ; eb 04
    mov byte [bp-002h], 000h                  ; c6 46 fe 00
    or byte [bp-002h], 005h                   ; 80 4e fe 05
    movzx bx, byte [bp-002h]                  ; 0f b6 5e fe
    mov ax, word [bp-008h]                    ; 8b 46 f8
    mov dx, word [bp-006h]                    ; 8b 56 fa
    call 07bdah                               ; e8 7c fd
    mov ax, word [bp-008h]                    ; 8b 46 f8
    add ax, 0026ah                            ; 05 6a 02
    mov dx, word [bp-006h]                    ; 8b 56 fa
    call 08f75h                               ; e8 0b 11
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    retn                                      ; c3
ahci_port_deinit_current_:                   ; 0xf7e6f LB 0x13f
    push bx                                   ; 53
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    enter 00006h, 000h                        ; c8 06 00 00
    mov di, ax                                ; 89 c7
    mov word [bp-004h], dx                    ; 89 56 fc
    mov es, dx                                ; 8e c2
    mov si, word [es:di+00260h]               ; 26 8b b5 60 02
    mov al, byte [es:di+00262h]               ; 26 8a 85 62 02
    mov byte [bp-002h], al                    ; 88 46 fe
    cmp AL, strict byte 0ffh                  ; 3c ff
    je near 07fa8h                            ; 0f 84 17 01
    movzx dx, al                              ; 0f b6 d0
    sal dx, 007h                              ; c1 e2 07
    add dx, 00118h                            ; 81 c2 18 01
    mov bx, strict word 00011h                ; bb 11 00
    xor cx, cx                                ; 31 c9
    mov ax, si                                ; 89 f0
    call 07b27h                               ; e8 82 fc
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    sal ax, 007h                              ; c1 e0 07
    mov word [bp-006h], ax                    ; 89 46 fa
    mov dx, ax                                ; 89 c2
    add dx, 00118h                            ; 81 c2 18 01
    mov bx, 0c011h                            ; bb 11 c0
    xor cx, cx                                ; 31 c9
    mov ax, si                                ; 89 f0
    call 07b6bh                               ; e8 ac fc
    cmp AL, strict byte 001h                  ; 3c 01
    je short 07ea5h                           ; 74 e2
    mov cx, strict word 00020h                ; b9 20 00
    xor bx, bx                                ; 31 db
    mov ax, di                                ; 89 f8
    mov dx, word [bp-004h]                    ; 8b 56 fc
    call 0901ah                               ; e8 4a 11
    lea ax, [di+00080h]                       ; 8d 85 80 00
    mov cx, strict word 00040h                ; b9 40 00
    xor bx, bx                                ; 31 db
    mov dx, word [bp-004h]                    ; 8b 56 fc
    call 0901ah                               ; e8 3b 11
    lea ax, [di+00200h]                       ; 8d 85 00 02
    mov cx, strict word 00060h                ; b9 60 00
    xor bx, bx                                ; 31 db
    mov dx, word [bp-004h]                    ; 8b 56 fc
    call 0901ah                               ; e8 2c 11
    mov ax, word [bp-006h]                    ; 8b 46 fa
    add ax, 00108h                            ; 05 08 01
    cwd                                       ; 99
    mov cx, dx                                ; 89 d1
    mov dx, si                                ; 89 f2
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    lea bx, [si+004h]                         ; 8d 5c 04
    xor ax, ax                                ; 31 c0
    xor cx, cx                                ; 31 c9
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov ax, word [bp-006h]                    ; 8b 46 fa
    add ax, 0010ch                            ; 05 0c 01
    cwd                                       ; 99
    mov cx, dx                                ; 89 d1
    mov dx, si                                ; 89 f2
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    xor ax, ax                                ; 31 c0
    xor cx, cx                                ; 31 c9
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov ax, word [bp-006h]                    ; 8b 46 fa
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4
    cwd                                       ; 99
    mov cx, dx                                ; 89 d1
    mov dx, si                                ; 89 f2
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    xor ax, ax                                ; 31 c0
    xor cx, cx                                ; 31 c9
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov ax, word [bp-006h]                    ; 8b 46 fa
    add ax, 00104h                            ; 05 04 01
    cwd                                       ; 99
    mov cx, dx                                ; 89 d1
    mov dx, si                                ; 89 f2
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    xor ax, ax                                ; 31 c0
    xor cx, cx                                ; 31 c9
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov ax, word [bp-006h]                    ; 8b 46 fa
    add ax, 00114h                            ; 05 14 01
    cwd                                       ; 99
    mov cx, dx                                ; 89 d1
    mov dx, si                                ; 89 f2
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    xor ax, ax                                ; 31 c0
    xor cx, cx                                ; 31 c9
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov es, [bp-004h]                         ; 8e 46 fc
    mov byte [es:di+00262h], 0ffh             ; 26 c6 85 62 02 ff
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
ahci_port_init_:                             ; 0xf7fae LB 0x201
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    enter 00006h, 000h                        ; c8 06 00 00
    mov si, ax                                ; 89 c6
    mov word [bp-004h], dx                    ; 89 56 fc
    mov byte [bp-002h], bl                    ; 88 5e fe
    call 07e6fh                               ; e8 af fe
    movzx dx, bl                              ; 0f b6 d3
    sal dx, 007h                              ; c1 e2 07
    add dx, 00118h                            ; 81 c2 18 01
    mov es, [bp-004h]                         ; 8e 46 fc
    mov ax, word [es:si+00260h]               ; 26 8b 84 60 02
    mov bx, strict word 00011h                ; bb 11 00
    xor cx, cx                                ; 31 c9
    call 07b27h                               ; e8 4d fb
    movzx di, byte [bp-002h]                  ; 0f b6 7e fe
    sal di, 007h                              ; c1 e7 07
    lea dx, [di+00118h]                       ; 8d 95 18 01
    mov es, [bp-004h]                         ; 8e 46 fc
    mov ax, word [es:si+00260h]               ; 26 8b 84 60 02
    mov bx, 0c011h                            ; bb 11 c0
    xor cx, cx                                ; 31 c9
    call 07b6bh                               ; e8 76 fb
    cmp AL, strict byte 001h                  ; 3c 01
    je short 07fdah                           ; 74 e1
    mov cx, strict word 00020h                ; b9 20 00
    xor bx, bx                                ; 31 db
    mov ax, si                                ; 89 f0
    mov dx, word [bp-004h]                    ; 8b 56 fc
    call 0901ah                               ; e8 14 10
    lea ax, [si+00080h]                       ; 8d 84 80 00
    mov cx, strict word 00040h                ; b9 40 00
    xor bx, bx                                ; 31 db
    mov dx, word [bp-004h]                    ; 8b 56 fc
    call 0901ah                               ; e8 05 10
    mov ax, si                                ; 89 f0
    add ah, 002h                              ; 80 c4 02
    mov word [bp-006h], ax                    ; 89 46 fa
    mov cx, strict word 00060h                ; b9 60 00
    xor bx, bx                                ; 31 db
    mov dx, word [bp-004h]                    ; 8b 56 fc
    call 0901ah                               ; e8 f2 0f
    lea ax, [di+00108h]                       ; 8d 85 08 01
    cwd                                       ; 99
    mov es, [bp-004h]                         ; 8e 46 fc
    mov bx, word [es:si+00260h]               ; 26 8b 9c 60 02
    mov cx, dx                                ; 89 d1
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov ax, word [bp-006h]                    ; 8b 46 fa
    mov dx, word [bp-004h]                    ; 8b 56 fc
    call 07bbch                               ; e8 71 fb
    mov es, [bp-004h]                         ; 8e 46 fc
    mov bx, word [es:si+00260h]               ; 26 8b 9c 60 02
    add bx, strict byte 00004h                ; 83 c3 04
    mov cx, dx                                ; 89 d1
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    lea ax, [di+0010ch]                       ; 8d 85 0c 01
    cwd                                       ; 99
    mov es, [bp-004h]                         ; 8e 46 fc
    mov bx, word [es:si+00260h]               ; 26 8b 9c 60 02
    mov cx, dx                                ; 89 d1
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov es, [bp-004h]                         ; 8e 46 fc
    mov dx, word [es:si+00260h]               ; 26 8b 94 60 02
    add dx, strict byte 00004h                ; 83 c2 04
    xor ax, ax                                ; 31 c0
    xor cx, cx                                ; 31 c9
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    lea ax, [di+00100h]                       ; 8d 85 00 01
    cwd                                       ; 99
    mov es, [bp-004h]                         ; 8e 46 fc
    mov bx, word [es:si+00260h]               ; 26 8b 9c 60 02
    mov cx, dx                                ; 89 d1
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov ax, si                                ; 89 f0
    mov dx, word [bp-004h]                    ; 8b 56 fc
    call 07bbch                               ; e8 05 fb
    mov es, [bp-004h]                         ; 8e 46 fc
    mov bx, word [es:si+00260h]               ; 26 8b 9c 60 02
    add bx, strict byte 00004h                ; 83 c3 04
    mov cx, dx                                ; 89 d1
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    lea ax, [di+00104h]                       ; 8d 85 04 01
    cwd                                       ; 99
    mov es, [bp-004h]                         ; 8e 46 fc
    mov bx, word [es:si+00260h]               ; 26 8b 9c 60 02
    mov cx, dx                                ; 89 d1
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov es, [bp-004h]                         ; 8e 46 fc
    mov dx, word [es:si+00260h]               ; 26 8b 94 60 02
    add dx, strict byte 00004h                ; 83 c2 04
    xor ax, ax                                ; 31 c0
    xor cx, cx                                ; 31 c9
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    lea ax, [di+00114h]                       ; 8d 85 14 01
    cwd                                       ; 99
    mov es, [bp-004h]                         ; 8e 46 fc
    mov bx, word [es:si+00260h]               ; 26 8b 9c 60 02
    mov cx, dx                                ; 89 d1
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov es, [bp-004h]                         ; 8e 46 fc
    mov dx, word [es:si+00260h]               ; 26 8b 94 60 02
    add dx, strict byte 00004h                ; 83 c2 04
    xor ax, ax                                ; 31 c0
    xor cx, cx                                ; 31 c9
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    lea ax, [di+00110h]                       ; 8d 85 10 01
    cwd                                       ; 99
    mov es, [bp-004h]                         ; 8e 46 fc
    mov bx, word [es:si+00260h]               ; 26 8b 9c 60 02
    mov cx, dx                                ; 89 d1
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov es, [bp-004h]                         ; 8e 46 fc
    mov dx, word [es:si+00260h]               ; 26 8b 94 60 02
    add dx, strict byte 00004h                ; 83 c2 04
    mov ax, strict word 0ffffh                ; b8 ff ff
    mov cx, ax                                ; 89 c1
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    lea ax, [di+00130h]                       ; 8d 85 30 01
    cwd                                       ; 99
    mov es, [bp-004h]                         ; 8e 46 fc
    mov bx, word [es:si+00260h]               ; 26 8b 9c 60 02
    mov cx, dx                                ; 89 d1
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov es, [bp-004h]                         ; 8e 46 fc
    mov dx, word [es:si+00260h]               ; 26 8b 94 60 02
    add dx, strict byte 00004h                ; 83 c2 04
    mov ax, strict word 0ffffh                ; b8 ff ff
    mov cx, ax                                ; 89 c1
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov al, byte [bp-002h]                    ; 8a 46 fe
    mov es, [bp-004h]                         ; 8e 46 fc
    mov byte [es:si+00262h], al               ; 26 88 84 62 02
    mov byte [es:si+00263h], 000h             ; 26 c6 84 63 02 00
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    retn                                      ; c3
@ahci_read_sectors:                          ; 0xf81af LB 0x91
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    les di, [bp+008h]                         ; c4 7e 08
    movzx di, byte [es:di+008h]               ; 26 0f b6 7d 08
    sub di, strict byte 0000ch                ; 83 ef 0c
    cmp di, strict byte 00004h                ; 83 ff 04
    jbe short 081d3h                          ; 76 0f
    push di                                   ; 57
    push 00bf6h                               ; 68 f6 0b
    push 00c08h                               ; 68 08 0c
    push strict byte 00007h                   ; 6a 07
    call 018e9h                               ; e8 19 97
    add sp, strict byte 00008h                ; 83 c4 08
    les bx, [bp+008h]                         ; c4 5e 08
    mov dx, word [es:bx+001eeh]               ; 26 8b 97 ee 01
    xor ax, ax                                ; 31 c0
    call 07abfh                               ; e8 df f8
    mov es, [bp+00ah]                         ; 8e 46 0a
    add di, bx                                ; 01 df
    movzx bx, byte [es:di+001e9h]             ; 26 0f b6 9d e9 01
    mov di, word [bp+008h]                    ; 8b 7e 08
    mov dx, word [es:di+001eeh]               ; 26 8b 95 ee 01
    xor ax, ax                                ; 31 c0
    call 07faeh                               ; e8 b6 fd
    mov bx, strict word 00025h                ; bb 25 00
    mov ax, di                                ; 89 f8
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    call 07caah                               ; e8 a7 fa
    mov es, [bp+00ah]                         ; 8e 46 0a
    mov bx, di                                ; 89 fb
    mov ax, word [es:bx+00ah]                 ; 26 8b 47 0a
    mov word [es:bx+014h], ax                 ; 26 89 47 14
    mov cx, ax                                ; 89 c1
    sal cx, 009h                              ; c1 e1 09
    shr cx, 1                                 ; d1 e9
    mov di, word [es:di+004h]                 ; 26 8b 7d 04
    mov ax, word [es:bx+006h]                 ; 26 8b 47 06
    mov si, di                                ; 89 fe
    mov dx, ax                                ; 89 c2
    mov es, ax                                ; 8e c0
    push DS                                   ; 1e
    mov ds, dx                                ; 8e da
    rep movsw                                 ; f3 a5
    pop DS                                    ; 1f
    mov es, [bp+00ah]                         ; 8e 46 0a
    mov dx, word [es:bx+001eeh]               ; 26 8b 97 ee 01
    xor ax, ax                                ; 31 c0
    call 07ad3h                               ; e8 9b f8
    xor ax, ax                                ; 31 c0
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn 00004h                               ; c2 04 00
@ahci_write_sectors:                         ; 0xf8240 LB 0x6d
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov si, word [bp+006h]                    ; 8b 76 06
    mov cx, word [bp+008h]                    ; 8b 4e 08
    mov es, cx                                ; 8e c1
    movzx bx, byte [es:si+008h]               ; 26 0f b6 5c 08
    sub bx, strict byte 0000ch                ; 83 eb 0c
    cmp bx, strict byte 00004h                ; 83 fb 04
    jbe short 08268h                          ; 76 0f
    push bx                                   ; 53
    push 00c27h                               ; 68 27 0c
    push 00c08h                               ; 68 08 0c
    push strict byte 00007h                   ; 6a 07
    call 018e9h                               ; e8 84 96
    add sp, strict byte 00008h                ; 83 c4 08
    mov es, cx                                ; 8e c1
    mov dx, word [es:si+001eeh]               ; 26 8b 94 ee 01
    xor ax, ax                                ; 31 c0
    call 07abfh                               ; e8 4b f8
    mov es, cx                                ; 8e c1
    add bx, si                                ; 01 f3
    movzx bx, byte [es:bx+001e9h]             ; 26 0f b6 9f e9 01
    mov dx, word [es:si+001eeh]               ; 26 8b 94 ee 01
    xor ax, ax                                ; 31 c0
    call 07faeh                               ; e8 26 fd
    mov bx, strict word 00035h                ; bb 35 00
    mov ax, si                                ; 89 f0
    mov dx, cx                                ; 89 ca
    call 07caah                               ; e8 18 fa
    mov es, cx                                ; 8e c1
    mov dx, word [es:si+00ah]                 ; 26 8b 54 0a
    mov word [es:si+014h], dx                 ; 26 89 54 14
    mov dx, word [es:si+001eeh]               ; 26 8b 94 ee 01
    xor ax, ax                                ; 31 c0
    call 07ad3h                               ; e8 2d f8
    xor ax, ax                                ; 31 c0
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn 00004h                               ; c2 04 00
ahci_cmd_packet_:                            ; 0xf82ad LB 0x16e
    push si                                   ; 56
    push di                                   ; 57
    enter 0000eh, 000h                        ; c8 0e 00 00
    push ax                                   ; 50
    mov byte [bp-002h], dl                    ; 88 56 fe
    mov word [bp-00eh], bx                    ; 89 5e f2
    mov word [bp-00ch], cx                    ; 89 4e f4
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 56 93
    mov si, 00122h                            ; be 22 01
    mov word [bp-004h], ax                    ; 89 46 fc
    cmp byte [bp+00eh], 002h                  ; 80 7e 0e 02
    jne short 082f1h                          ; 75 1f
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 d1 95
    push 00c3ah                               ; 68 3a 0c
    push 00c4ah                               ; 68 4a 0c
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 01 96
    add sp, strict byte 00006h                ; 83 c4 06
    mov ax, strict word 00001h                ; b8 01 00
    jmp near 08415h                           ; e9 24 01
    test byte [bp+008h], 001h                 ; f6 46 08 01
    jne short 082ebh                          ; 75 f4
    mov ax, word [bp+00ah]                    ; 8b 46 0a
    mov dx, word [bp+00ch]                    ; 8b 56 0c
    mov cx, strict word 00008h                ; b9 08 00
    sal ax, 1                                 ; d1 e0
    rcl dx, 1                                 ; d1 d2
    loop 08300h                               ; e2 fa
    mov es, [bp-004h]                         ; 8e 46 fc
    mov word [es:si], ax                      ; 26 89 04
    mov word [es:si+002h], dx                 ; 26 89 54 02
    mov ax, word [bp+010h]                    ; 8b 46 10
    mov word [es:si+004h], ax                 ; 26 89 44 04
    mov ax, word [bp+012h]                    ; 8b 46 12
    mov word [es:si+006h], ax                 ; 26 89 44 06
    mov bx, word [es:si+00ch]                 ; 26 8b 5c 0c
    mov ax, word [bp+00ah]                    ; 8b 46 0a
    mov dx, word [bp+00ch]                    ; 8b 56 0c
    xor cx, cx                                ; 31 c9
    call 08fb0h                               ; e8 83 0c
    mov word [es:si+00ah], ax                 ; 26 89 44 0a
    xor di, di                                ; 31 ff
    mov ax, word [es:si+001eeh]               ; 26 8b 84 ee 01
    mov word [bp-006h], ax                    ; 89 46 fa
    mov word [bp-00ah], di                    ; 89 7e f6
    mov word [bp-008h], ax                    ; 89 46 f8
    sub word [bp-010h], strict byte 0000ch    ; 83 6e f0 0c
    xor ax, ax                                ; 31 c0
    mov dx, word [bp-006h]                    ; 8b 56 fa
    call 07abfh                               ; e8 72 f7
    mov es, [bp-004h]                         ; 8e 46 fc
    mov bx, word [bp-010h]                    ; 8b 5e f0
    add bx, si                                ; 01 f3
    movzx bx, byte [es:bx+001e9h]             ; 26 0f b6 9f e9 01
    mov dx, word [es:si+001eeh]               ; 26 8b 94 ee 01
    xor ax, ax                                ; 31 c0
    call 07faeh                               ; e8 49 fc
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    push ax                                   ; 50
    mov bx, word [bp-00eh]                    ; 8b 5e f2
    mov cx, word [bp-00ch]                    ; 8b 4e f4
    mov ax, 000c0h                            ; b8 c0 00
    mov dx, word [bp-006h]                    ; 8b 56 fa
    call 09027h                               ; e8 ae 0c
    mov es, [bp-004h]                         ; 8e 46 fc
    mov word [es:si+014h], di                 ; 26 89 7c 14
    mov word [es:si+016h], di                 ; 26 89 7c 16
    mov word [es:si+018h], di                 ; 26 89 7c 18
    mov ax, word [es:si+01ah]                 ; 26 8b 44 1a
    test ax, ax                               ; 85 c0
    je short 083b7h                           ; 74 27
    dec ax                                    ; 48
    mov es, [bp-006h]                         ; 8e 46 fa
    mov word [es:di+0010ch], ax               ; 26 89 85 0c 01
    mov word [es:di+0010eh], di               ; 26 89 bd 0e 01
    mov ax, word [es:di+00264h]               ; 26 8b 85 64 02
    mov dx, word [es:di+00266h]               ; 26 8b 95 66 02
    mov word [es:di+00100h], ax               ; 26 89 85 00 01
    mov word [es:di+00102h], dx               ; 26 89 95 02 01
    inc byte [es:di+00263h]                   ; 26 fe 85 63 02
    mov bx, 000a0h                            ; bb a0 00
    mov ax, si                                ; 89 f0
    mov dx, word [bp-004h]                    ; 8b 56 fc
    call 07caah                               ; e8 e8 f8
    les bx, [bp-00ah]                         ; c4 5e f6
    mov ax, word [es:bx+004h]                 ; 26 8b 47 04
    mov dx, word [es:bx+006h]                 ; 26 8b 57 06
    mov es, [bp-004h]                         ; 8e 46 fc
    mov word [es:si+016h], ax                 ; 26 89 44 16
    mov word [es:si+018h], dx                 ; 26 89 54 18
    mov bx, word [es:si+016h]                 ; 26 8b 5c 16
    mov cx, dx                                ; 89 d1
    shr cx, 1                                 ; d1 e9
    rcr bx, 1                                 ; d1 db
    mov di, word [es:si+004h]                 ; 26 8b 7c 04
    mov ax, word [es:si+006h]                 ; 26 8b 44 06
    mov cx, bx                                ; 89 d9
    mov si, di                                ; 89 fe
    mov dx, ax                                ; 89 c2
    mov es, ax                                ; 8e c0
    push DS                                   ; 1e
    mov ds, dx                                ; 8e da
    rep movsw                                 ; f3 a5
    pop DS                                    ; 1f
    mov ax, word [bp-00ah]                    ; 8b 46 f6
    mov dx, word [bp-008h]                    ; 8b 56 f8
    call 07ad3h                               ; e8 d2 f6
    les bx, [bp-00ah]                         ; c4 5e f6
    mov ax, word [es:bx+006h]                 ; 26 8b 47 06
    or ax, word [es:bx+004h]                  ; 26 0b 47 04
    jne short 08413h                          ; 75 05
    mov ax, strict word 00004h                ; b8 04 00
    jmp short 08415h                          ; eb 02
    xor ax, ax                                ; 31 c0
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn 0000ch                               ; c2 0c 00
ahci_port_detect_device_:                    ; 0xf841b LB 0x3cb
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    enter 00222h, 000h                        ; c8 22 02 00
    mov di, ax                                ; 89 c7
    mov word [bp-00ah], dx                    ; 89 56 f6
    mov byte [bp-002h], bl                    ; 88 5e fe
    movzx cx, bl                              ; 0f b6 cb
    mov bx, cx                                ; 89 cb
    call 07faeh                               ; e8 7c fb
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 e1 91
    mov word [bp-01ah], ax                    ; 89 46 e6
    mov si, 00122h                            ; be 22 01
    mov word [bp-008h], ax                    ; 89 46 f8
    mov word [bp-01ch], si                    ; 89 76 e4
    mov word [bp-00ch], ax                    ; 89 46 f4
    sal cx, 007h                              ; c1 e1 07
    mov word [bp-016h], cx                    ; 89 4e ea
    mov ax, cx                                ; 89 c8
    add ax, 0012ch                            ; 05 2c 01
    cwd                                       ; 99
    mov word [bp-018h], ax                    ; 89 46 e8
    mov bx, dx                                ; 89 d3
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov dx, word [es:di+00260h]               ; 26 8b 95 60 02
    mov cx, bx                                ; 89 d9
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov dx, word [es:di+00260h]               ; 26 8b 95 60 02
    add dx, strict byte 00004h                ; 83 c2 04
    mov ax, strict word 00001h                ; b8 01 00
    xor cx, cx                                ; 31 c9
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov dx, word [es:di+00260h]               ; 26 8b 95 60 02
    mov ax, word [bp-018h]                    ; 8b 46 e8
    mov cx, bx                                ; 89 d9
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov dx, word [es:di+00260h]               ; 26 8b 95 60 02
    add dx, strict byte 00004h                ; 83 c2 04
    xor ax, ax                                ; 31 c0
    xor cx, cx                                ; 31 c9
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov ax, word [bp-016h]                    ; 8b 46 ea
    add ax, 00128h                            ; 05 28 01
    cwd                                       ; 99
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov bx, word [es:di+00260h]               ; 26 8b 9d 60 02
    mov cx, dx                                ; 89 d1
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov dx, word [es:di+00260h]               ; 26 8b 95 60 02
    add dx, strict byte 00004h                ; 83 c2 04
    in eax, DX                                ; 66 ed
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0
    shr eax, 010h                             ; 66 c1 e8 10
    xchg dx, ax                               ; 92
    push strict byte 00000h                   ; 6a 00
    mov bx, strict word 0000fh                ; bb 0f 00
    xor cx, cx                                ; 31 c9
    call 07ba1h                               ; e8 b2 f6
    cmp ax, strict word 00003h                ; 3d 03 00
    jne near 087e1h                           ; 0f 85 eb 02
    mov es, [bp-008h]                         ; 8e 46 f8
    mov al, byte [es:si+001edh]               ; 26 8a 84 ed 01
    mov byte [bp-004h], al                    ; 88 46 fc
    cmp AL, strict byte 004h                  ; 3c 04
    jnc near 087e1h                           ; 0f 83 da 02
    mov dx, word [bp-016h]                    ; 8b 56 ea
    add dx, 00118h                            ; 81 c2 18 01
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov ax, word [es:di+00260h]               ; 26 8b 85 60 02
    mov bx, strict word 00010h                ; bb 10 00
    xor cx, cx                                ; 31 c9
    call 07ae7h                               ; e8 c9 f5
    mov ax, word [bp-016h]                    ; 8b 46 ea
    add ax, 00124h                            ; 05 24 01
    cwd                                       ; 99
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov bx, word [es:di+00260h]               ; 26 8b 9d 60 02
    mov cx, dx                                ; 89 d1
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov dx, word [es:di+00260h]               ; 26 8b 95 60 02
    add dx, strict byte 00004h                ; 83 c2 04
    in eax, DX                                ; 66 ed
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0
    shr eax, 010h                             ; 66 c1 e8 10
    xchg dx, ax                               ; 92
    mov bx, ax                                ; 89 c3
    mov ax, dx                                ; 89 d0
    mov cl, byte [bp-004h]                    ; 8a 4e fc
    add cl, 00ch                              ; 80 c1 0c
    test dx, dx                               ; 85 d2
    jne near 08733h                           ; 0f 85 d5 01
    cmp bx, 00101h                            ; 81 fb 01 01
    jne near 08733h                           ; 0f 85 cd 01
    mov es, [bp-008h]                         ; 8e 46 f8
    db  066h, 026h, 0c7h, 004h, 000h, 000h, 000h, 000h
    ; mov dword [es:si], strict dword 000000000h ; 66 26 c7 04 00 00 00 00
    lea dx, [bp-00222h]                       ; 8d 96 de fd
    mov word [es:si+004h], dx                 ; 26 89 54 04
    mov [es:si+006h], ss                      ; 26 8c 54 06
    db  066h, 026h, 0c7h, 044h, 00ah, 001h, 000h, 000h, 002h
    ; mov dword [es:si+00ah], strict dword 002000001h ; 66 26 c7 44 0a 01 00 00 02
    mov bx, 000ech                            ; bb ec 00
    mov ax, si                                ; 89 f0
    mov dx, word [bp-01ah]                    ; 8b 56 e6
    call 07caah                               ; e8 19 f7
    mov byte [bp-006h], cl                    ; 88 4e fa
    test byte [bp-00222h], 080h               ; f6 86 de fd 80
    db  00fh, 095h, 0c0h
    ; setne al                                  ; 0f 95 c0
    xor ah, ah                                ; 30 e4
    mov dx, word [bp-00220h]                  ; 8b 96 e0 fd
    mov word [bp-014h], dx                    ; 89 56 ec
    mov dx, word [bp-0021ch]                  ; 8b 96 e4 fd
    mov word [bp-012h], dx                    ; 89 56 ee
    mov dx, word [bp-00216h]                  ; 8b 96 ea fd
    mov word [bp-010h], dx                    ; 89 56 f0
    mov di, word [bp-001aah]                  ; 8b be 56 fe
    mov dx, word [bp-001a8h]                  ; 8b 96 58 fe
    mov word [bp-00eh], dx                    ; 89 56 f2
    cmp dx, 00fffh                            ; 81 fa ff 0f
    jne short 085d4h                          ; 75 10
    cmp di, strict byte 0ffffh                ; 83 ff ff
    jne short 085d4h                          ; 75 0b
    mov di, word [bp-0015ah]                  ; 8b be a6 fe
    mov dx, word [bp-00158h]                  ; 8b 96 a8 fe
    mov word [bp-00eh], dx                    ; 89 56 f2
    movzx bx, byte [bp-004h]                  ; 0f b6 5e fc
    mov es, [bp-00ch]                         ; 8e 46 f4
    add bx, word [bp-01ch]                    ; 03 5e e4
    mov ah, byte [bp-002h]                    ; 8a 66 fe
    mov byte [es:bx+001e9h], ah               ; 26 88 a7 e9 01
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa
    imul dx, dx, strict byte 00018h           ; 6b d2 18
    mov si, word [bp-01ch]                    ; 8b 76 e4
    add si, dx                                ; 01 d6
    mov word [es:si+01eh], 0ff05h             ; 26 c7 44 1e 05 ff
    mov byte [es:si+020h], al                 ; 26 88 44 20
    mov byte [es:si+021h], 000h               ; 26 c6 44 21 00
    mov word [es:si+024h], 00200h             ; 26 c7 44 24 00 02
    mov byte [es:si+023h], 001h               ; 26 c6 44 23 01
    mov word [es:si+032h], di                 ; 26 89 7c 32
    mov ax, word [bp-00eh]                    ; 8b 46 f2
    mov word [es:si+034h], ax                 ; 26 89 44 34
    mov ax, word [bp-012h]                    ; 8b 46 ee
    mov word [es:si+02ch], ax                 ; 26 89 44 2c
    mov ax, word [bp-014h]                    ; 8b 46 ec
    mov word [es:si+02eh], ax                 ; 26 89 44 2e
    mov ax, word [bp-010h]                    ; 8b 46 f0
    mov word [es:si+030h], ax                 ; 26 89 44 30
    mov al, byte [bp-004h]                    ; 8a 46 fc
    cmp AL, strict byte 001h                  ; 3c 01
    jc short 0863fh                           ; 72 0c
    jbe short 08647h                          ; 76 12
    cmp AL, strict byte 003h                  ; 3c 03
    je short 0864fh                           ; 74 16
    cmp AL, strict byte 002h                  ; 3c 02
    je short 0864bh                           ; 74 0e
    jmp short 08698h                          ; eb 59
    test al, al                               ; 84 c0
    jne short 08698h                          ; 75 55
    mov DL, strict byte 040h                  ; b2 40
    jmp short 08651h                          ; eb 0a
    mov DL, strict byte 048h                  ; b2 48
    jmp short 08651h                          ; eb 06
    mov DL, strict byte 050h                  ; b2 50
    jmp short 08651h                          ; eb 02
    mov DL, strict byte 058h                  ; b2 58
    mov al, dl                                ; 88 d0
    add AL, strict byte 007h                  ; 04 07
    movzx bx, al                              ; 0f b6 d8
    mov ax, bx                                ; 89 d8
    call 0165ch                               ; e8 ff 8f
    test al, al                               ; 84 c0
    je short 08698h                           ; 74 37
    mov al, dl                                ; 88 d0
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    xor ah, ah                                ; 30 e4
    call 0165ch                               ; e8 f2 8f
    xor ah, ah                                ; 30 e4
    mov cx, ax                                ; 89 c1
    sal cx, 008h                              ; c1 e1 08
    movzx ax, dl                              ; 0f b6 c2
    call 0165ch                               ; e8 e5 8f
    xor ah, ah                                ; 30 e4
    add ax, cx                                ; 01 c8
    mov word [bp-020h], ax                    ; 89 46 e0
    mov al, dl                                ; 88 d0
    add AL, strict byte 002h                  ; 04 02
    xor ah, ah                                ; 30 e4
    call 0165ch                               ; e8 d5 8f
    xor ah, ah                                ; 30 e4
    mov word [bp-022h], ax                    ; 89 46 de
    mov ax, bx                                ; 89 d8
    call 0165ch                               ; e8 cb 8f
    xor ah, ah                                ; 30 e4
    mov word [bp-01eh], ax                    ; 89 46 e2
    jmp short 086a5h                          ; eb 0d
    mov bx, di                                ; 89 fb
    mov cx, word [bp-00eh]                    ; 8b 4e f2
    mov dx, ss                                ; 8c d2
    lea ax, [bp-022h]                         ; 8d 46 de
    call 052a4h                               ; e8 ff cb
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 fe 91
    push word [bp-00eh]                       ; ff 76 f2
    push di                                   ; 57
    mov ax, word [bp-01eh]                    ; 8b 46 e2
    push ax                                   ; 50
    mov ax, word [bp-022h]                    ; 8b 46 de
    push ax                                   ; 50
    mov ax, word [bp-020h]                    ; 8b 46 e0
    push ax                                   ; 50
    push dword [bp-012h]                      ; 66 ff 76 ee
    push word [bp-014h]                       ; ff 76 ec
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    push ax                                   ; 50
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc
    push ax                                   ; 50
    push 00c6ah                               ; 68 6a 0c
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 10 92
    add sp, strict byte 00018h                ; 83 c4 18
    movzx di, byte [bp-006h]                  ; 0f b6 7e fa
    imul di, di, strict byte 00018h           ; 6b ff 18
    add di, word [bp-01ch]                    ; 03 7e e4
    mov es, [bp-00ch]                         ; 8e 46 f4
    lea di, [di+026h]                         ; 8d 7d 26
    push DS                                   ; 1e
    push SS                                   ; 16
    pop DS                                    ; 1f
    lea si, [bp-022h]                         ; 8d 76 de
    movsw                                     ; a5
    movsw                                     ; a5
    movsw                                     ; a5
    pop DS                                    ; 1f
    mov bx, word [bp-01ch]                    ; 8b 5e e4
    mov al, byte [es:bx+0019eh]               ; 26 8a 87 9e 01
    mov ah, byte [bp-004h]                    ; 8a 66 fc
    add ah, 00ch                              ; 80 c4 0c
    movzx bx, al                              ; 0f b6 d8
    add bx, word [bp-01ch]                    ; 03 5e e4
    mov byte [es:bx+0019fh], ah               ; 26 88 a7 9f 01
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    mov bx, word [bp-01ch]                    ; 8b 5e e4
    mov byte [es:bx+0019eh], al               ; 26 88 87 9e 01
    mov dx, strict word 00075h                ; ba 75 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 de 8e
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 00075h                ; ba 75 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 de 8e
    jmp near 087d0h                           ; e9 9d 00
    cmp dx, 0eb14h                            ; 81 fa 14 eb
    jne near 087d0h                           ; 0f 85 95 00
    cmp bx, 00101h                            ; 81 fb 01 01
    jne near 087d0h                           ; 0f 85 8d 00
    mov es, [bp-008h]                         ; 8e 46 f8
    db  066h, 026h, 0c7h, 004h, 000h, 000h, 000h, 000h
    ; mov dword [es:si], strict dword 000000000h ; 66 26 c7 04 00 00 00 00
    lea dx, [bp-00222h]                       ; 8d 96 de fd
    mov word [es:si+004h], dx                 ; 26 89 54 04
    mov [es:si+006h], ss                      ; 26 8c 54 06
    db  066h, 026h, 0c7h, 044h, 00ah, 001h, 000h, 000h, 002h
    ; mov dword [es:si+00ah], strict dword 002000001h ; 66 26 c7 44 0a 01 00 00 02
    mov bx, 000a1h                            ; bb a1 00
    mov ax, si                                ; 89 f0
    mov dx, word [bp-01ah]                    ; 8b 56 e6
    call 07caah                               ; e8 3c f5
    test byte [bp-00222h], 080h               ; f6 86 de fd 80
    db  00fh, 095h, 0c0h
    ; setne al                                  ; 0f 95 c0
    xor ah, ah                                ; 30 e4
    movzx bx, byte [bp-004h]                  ; 0f b6 5e fc
    mov es, [bp-01ah]                         ; 8e 46 e6
    add bx, si                                ; 01 f3
    mov ah, byte [bp-002h]                    ; 8a 66 fe
    mov byte [es:bx+001e9h], ah               ; 26 88 a7 e9 01
    movzx si, cl                              ; 0f b6 f1
    imul si, si, strict byte 00018h           ; 6b f6 18
    add si, 00122h                            ; 81 c6 22 01
    mov word [es:si+01eh], 00505h             ; 26 c7 44 1e 05 05
    mov byte [es:si+020h], al                 ; 26 88 44 20
    mov word [es:si+024h], 00800h             ; 26 c7 44 24 00 08
    mov es, [bp-00ch]                         ; 8e 46 f4
    mov bx, word [bp-01ch]                    ; 8b 5e e4
    mov al, byte [es:bx+001afh]               ; 26 8a 87 af 01
    mov ah, byte [bp-004h]                    ; 8a 66 fc
    add ah, 00ch                              ; 80 c4 0c
    movzx bx, al                              ; 0f b6 d8
    mov es, [bp-01ah]                         ; 8e 46 e6
    add bx, 00122h                            ; 81 c3 22 01
    mov byte [es:bx+001b0h], ah               ; 26 88 a7 b0 01
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    mov es, [bp-00ch]                         ; 8e 46 f4
    mov bx, word [bp-01ch]                    ; 8b 5e e4
    mov byte [es:bx+001afh], al               ; 26 88 87 af 01
    inc byte [bp-004h]                        ; fe 46 fc
    mov al, byte [bp-004h]                    ; 8a 46 fc
    mov es, [bp-00ch]                         ; 8e 46 f4
    mov bx, word [bp-01ch]                    ; 8b 5e e4
    mov byte [es:bx+001edh], al               ; 26 88 87 ed 01
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    retn                                      ; c3
ahci_mem_alloc_:                             ; 0xf87e6 LB 0x40
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, 00413h                            ; ba 13 04
    xor ax, ax                                ; 31 c0
    call 0161ch                               ; e8 26 8e
    test ax, ax                               ; 85 c0
    je short 0881fh                           ; 74 25
    dec ax                                    ; 48
    mov bx, ax                                ; 89 c3
    xor dx, dx                                ; 31 d2
    mov cx, strict word 0000ah                ; b9 0a 00
    sal ax, 1                                 ; d1 e0
    rcl dx, 1                                 ; d1 d2
    loop 08802h                               ; e2 fa
    mov si, ax                                ; 89 c6
    mov di, dx                                ; 89 d7
    mov cx, strict word 00004h                ; b9 04 00
    shr di, 1                                 ; d1 ef
    rcr si, 1                                 ; d1 de
    loop 0880fh                               ; e2 fa
    mov dx, 00413h                            ; ba 13 04
    xor ax, ax                                ; 31 c0
    call 0162ah                               ; e8 0d 8e
    mov ax, si                                ; 89 f0
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
ahci_hba_init_:                              ; 0xf8826 LB 0x120
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push si                                   ; 56
    push di                                   ; 57
    enter 00006h, 000h                        ; c8 06 00 00
    mov si, ax                                ; 89 c6
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 e2 8d
    mov bx, 00122h                            ; bb 22 01
    mov word [bp-006h], ax                    ; 89 46 fa
    mov ax, strict word 00010h                ; b8 10 00
    xor cx, cx                                ; 31 c9
    mov dx, si                                ; 89 f2
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    lea dx, [si+004h]                         ; 8d 54 04
    in eax, DX                                ; 66 ed
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0
    shr eax, 010h                             ; 66 c1 e8 10
    xchg dx, ax                               ; 92
    call 087e6h                               ; e8 87 ff
    mov di, ax                                ; 89 c7
    test ax, ax                               ; 85 c0
    je near 08928h                            ; 0f 84 c1 00
    mov es, [bp-006h]                         ; 8e 46 fa
    mov word [es:bx+001eeh], di               ; 26 89 bf ee 01
    mov byte [es:bx+001edh], 000h             ; 26 c6 87 ed 01 00
    xor bx, bx                                ; 31 db
    mov es, di                                ; 8e c7
    mov byte [es:bx+00262h], 0ffh             ; 26 c6 87 62 02 ff
    mov word [es:bx+00260h], si               ; 26 89 b7 60 02
    db  066h, 026h, 0c7h, 087h, 064h, 002h, 000h, 0c0h, 00ch, 000h
    ; mov dword [es:bx+00264h], strict dword 0000cc000h ; 66 26 c7 87 64 02 00 c0 0c 00
    mov bx, strict word 00001h                ; bb 01 00
    xor cx, cx                                ; 31 c9
    mov dx, strict word 00004h                ; ba 04 00
    mov ax, si                                ; 89 f0
    call 07ae7h                               ; e8 4c f2
    mov ax, strict word 00004h                ; b8 04 00
    xor cx, cx                                ; 31 c9
    mov dx, si                                ; 89 f2
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    lea bx, [si+004h]                         ; 8d 5c 04
    mov dx, bx                                ; 89 da
    in eax, DX                                ; 66 ed
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0
    shr eax, 010h                             ; 66 c1 e8 10
    xchg dx, ax                               ; 92
    test AL, strict byte 001h                 ; a8 01
    jne short 0889bh                          ; 75 de
    xor ax, ax                                ; 31 c0
    xor cx, cx                                ; 31 c9
    mov dx, si                                ; 89 f2
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov dx, bx                                ; 89 da
    in eax, DX                                ; 66 ed
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0
    shr eax, 010h                             ; 66 c1 e8 10
    xchg dx, ax                               ; 92
    push strict byte 00000h                   ; 6a 00
    mov bx, strict word 0001fh                ; bb 1f 00
    xor cx, cx                                ; 31 c9
    call 07ba1h                               ; e8 c0 f2
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    mov byte [bp-004h], al                    ; 88 46 fc
    mov byte [bp-002h], 000h                  ; c6 46 fe 00
    jmp short 088f5h                          ; eb 09
    inc byte [bp-002h]                        ; fe 46 fe
    cmp byte [bp-002h], 020h                  ; 80 7e fe 20
    jnc short 08926h                          ; 73 31
    movzx cx, byte [bp-002h]                  ; 0f b6 4e fe
    mov ax, strict word 00001h                ; b8 01 00
    xor dx, dx                                ; 31 d2
    jcxz 08906h                               ; e3 06
    sal ax, 1                                 ; d1 e0
    rcl dx, 1                                 ; d1 d2
    loop 08900h                               ; e2 fa
    mov bx, ax                                ; 89 c3
    mov cx, dx                                ; 89 d1
    mov dx, strict word 0000ch                ; ba 0c 00
    mov ax, si                                ; 89 f0
    call 07b6bh                               ; e8 59 f2
    test al, al                               ; 84 c0
    je short 088ech                           ; 74 d6
    movzx bx, byte [bp-002h]                  ; 0f b6 5e fe
    xor ax, ax                                ; 31 c0
    mov dx, di                                ; 89 fa
    call 0841bh                               ; e8 fa fa
    dec byte [bp-004h]                        ; fe 4e fc
    jne short 088ech                          ; 75 c6
    xor ax, ax                                ; 31 c0
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
    db  00bh, 005h, 004h, 003h, 002h, 001h, 000h, 013h, 08ah, 0f1h, 089h, 0f7h, 089h, 0fdh, 089h, 003h
    db  08ah, 009h, 08ah, 00fh, 08ah, 013h, 08ah
_ahci_init:                                  ; 0xf8946 LB 0xf9
    push si                                   ; 56
    push di                                   ; 57
    enter 00006h, 000h                        ; c8 06 00 00
    mov ax, 00601h                            ; b8 01 06
    mov dx, strict word 00001h                ; ba 01 00
    call 08e38h                               ; e8 e3 04
    mov dx, ax                                ; 89 c2
    cmp ax, strict word 0ffffh                ; 3d ff ff
    je near 08a3bh                            ; 0f 84 dd 00
    xor al, al                                ; 30 c0
    shr ax, 008h                              ; c1 e8 08
    mov byte [bp-006h], al                    ; 88 46 fa
    mov byte [bp-004h], dl                    ; 88 56 fc
    xor dh, dh                                ; 30 f6
    xor ah, ah                                ; 30 e4
    mov bx, strict word 00034h                ; bb 34 00
    call 08e60h                               ; e8 ed 04
    mov cl, al                                ; 88 c1
    test cl, cl                               ; 84 c9
    je short 0899ch                           ; 74 23
    movzx bx, cl                              ; 0f b6 d9
    movzx di, byte [bp-004h]                  ; 0f b6 7e fc
    movzx si, byte [bp-006h]                  ; 0f b6 76 fa
    mov dx, di                                ; 89 fa
    mov ax, si                                ; 89 f0
    call 08e60h                               ; e8 d5 04
    cmp AL, strict byte 012h                  ; 3c 12
    je short 0899ch                           ; 74 0d
    mov al, cl                                ; 88 c8
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    movzx bx, al                              ; 0f b6 d8
    mov dx, di                                ; 89 fa
    mov ax, si                                ; 89 f0
    jmp short 08970h                          ; eb d4
    test cl, cl                               ; 84 c9
    je near 08a3bh                            ; 0f 84 99 00
    add cl, 002h                              ; 80 c1 02
    movzx bx, cl                              ; 0f b6 d9
    movzx di, byte [bp-004h]                  ; 0f b6 7e fc
    movzx si, byte [bp-006h]                  ; 0f b6 76 fa
    mov dx, di                                ; 89 fa
    mov ax, si                                ; 89 f0
    call 08e60h                               ; e8 a9 04
    cmp AL, strict byte 010h                  ; 3c 10
    jne near 08a3bh                           ; 0f 85 7e 00
    mov byte [bp-002h], 000h                  ; c6 46 fe 00
    mov al, cl                                ; 88 c8
    add AL, strict byte 002h                  ; 04 02
    movzx bx, al                              ; 0f b6 d8
    mov dx, di                                ; 89 fa
    mov ax, si                                ; 89 f0
    call 08e81h                               ; e8 b2 04
    mov dx, ax                                ; 89 c2
    and ax, strict word 0000fh                ; 25 0f 00
    sub ax, strict word 00004h                ; 2d 04 00
    cmp ax, strict word 0000bh                ; 3d 0b 00
    jnbe short 08a13h                         ; 77 37
    push CS                                   ; 0e
    pop ES                                    ; 07
    mov cx, strict word 00008h                ; b9 08 00
    mov di, 0892fh                            ; bf 2f 89
    repne scasb                               ; f2 ae
    sal cx, 1                                 ; d1 e1
    mov di, cx                                ; 89 cf
    mov ax, word [cs:di-076cah]               ; 2e 8b 85 36 89
    jmp ax                                    ; ff e0
    mov byte [bp-002h], 010h                  ; c6 46 fe 10
    jmp short 08a13h                          ; eb 1c
    mov byte [bp-002h], 014h                  ; c6 46 fe 14
    jmp short 08a13h                          ; eb 16
    mov byte [bp-002h], 018h                  ; c6 46 fe 18
    jmp short 08a13h                          ; eb 10
    mov byte [bp-002h], 01ch                  ; c6 46 fe 1c
    jmp short 08a13h                          ; eb 0a
    mov byte [bp-002h], 020h                  ; c6 46 fe 20
    jmp short 08a13h                          ; eb 04
    mov byte [bp-002h], 024h                  ; c6 46 fe 24
    mov si, dx                                ; 89 d6
    shr si, 004h                              ; c1 ee 04
    sal si, 002h                              ; c1 e6 02
    mov al, byte [bp-002h]                    ; 8a 46 fe
    test al, al                               ; 84 c0
    je short 08a3bh                           ; 74 19
    movzx bx, al                              ; 0f b6 d8
    movzx dx, byte [bp-004h]                  ; 0f b6 56 fc
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa
    call 08ea0h                               ; e8 70 04
    test AL, strict byte 001h                 ; a8 01
    je short 08a3bh                           ; 74 07
    and AL, strict byte 0f0h                  ; 24 f0
    add ax, si                                ; 01 f0
    call 08826h                               ; e8 eb fd
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
apm_out_str_:                                ; 0xf8a3f LB 0x36
    push bx                                   ; 53
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, ax                                ; 89 c3
    cmp byte [bx], 000h                       ; 80 3f 00
    je short 08a54h                           ; 74 0a
    mov al, byte [bx]                         ; 8a 07
    out DX, AL                                ; ee
    inc bx                                    ; 43
    mov al, byte [bx]                         ; 8a 07
    db  00ah, 0c0h
    ; or al, al                                 ; 0a c0
    jne short 08a4ch                          ; 75 f8
    pop bp                                    ; 5d
    pop bx                                    ; 5b
    retn                                      ; c3
    db  098h, 08ah, 067h, 08bh, 0aah, 08ah, 0c5h, 08ah, 067h, 08bh, 0f0h, 08ah, 067h, 08bh, 0f5h, 08ah
    db  03ch, 08bh, 03ch, 08bh, 03ch, 08bh, 037h, 08bh, 03ch, 08bh, 03ch, 08bh, 02fh, 08bh
_apm_function:                               ; 0xf8a75 LB 0xf5
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    and byte [bp+01ah], 0feh                  ; 80 66 1a fe
    mov ax, word [bp+014h]                    ; 8b 46 14
    xor ah, ah                                ; 30 e4
    cmp ax, strict word 0000eh                ; 3d 0e 00
    jnbe near 08b3ch                          ; 0f 87 b3 00
    mov bx, ax                                ; 89 c3
    add bx, ax                                ; 01 c3
    mov dx, word [bp+01ah]                    ; 8b 56 1a
    or dl, 001h                               ; 80 ca 01
    jmp word [cs:bx-075a9h]                   ; 2e ff a7 57 8a
    mov word [bp+014h], 00102h                ; c7 46 14 02 01
    mov word [bp+00eh], 0504dh                ; c7 46 0e 4d 50
    mov word [bp+012h], strict word 00003h    ; c7 46 12 03 00
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn                                      ; c3
    mov word [bp+014h], 0f000h                ; c7 46 14 00 f0
    mov word [bp+00eh], 09094h                ; c7 46 0e 94 90
    mov word [bp+012h], 0f000h                ; c7 46 12 00 f0
    mov ax, strict word 0fff0h                ; b8 f0 ff
    mov word [bp+008h], ax                    ; 89 46 08
    mov word [bp+006h], ax                    ; 89 46 06
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn                                      ; c3
    mov word [bp+014h], 0f000h                ; c7 46 14 00 f0
    mov word [bp+00eh], 0da40h                ; c7 46 0e 40 da
    mov ax, 0f000h                            ; b8 00 f0
    mov word [bp+012h], ax                    ; 89 46 12
    mov word [bp+010h], ax                    ; 89 46 10
    mov ax, strict word 0fff0h                ; b8 f0 ff
    mov word [bp+008h], ax                    ; 89 46 08
    mov word [bp+006h], ax                    ; 89 46 06
    xor bx, bx                                ; 31 db
    sal ebx, 010h                             ; 66 c1 e3 10
    mov si, ax                                ; 89 c6
    sal esi, 010h                             ; 66 c1 e6 10
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn                                      ; c3
    sti                                       ; fb
    hlt                                       ; f4
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn                                      ; c3
    cmp word [bp+012h], strict byte 00003h    ; 83 7e 12 03
    je short 08b1bh                           ; 74 20
    cmp word [bp+012h], strict byte 00002h    ; 83 7e 12 02
    je short 08b13h                           ; 74 12
    cmp word [bp+012h], strict byte 00001h    ; 83 7e 12 01
    jne short 08b23h                          ; 75 1c
    mov dx, 08900h                            ; ba 00 89
    mov ax, 00ca2h                            ; b8 a2 0c
    call 08a3fh                               ; e8 2f ff
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn                                      ; c3
    mov dx, 08900h                            ; ba 00 89
    mov ax, 00caah                            ; b8 aa 0c
    jmp short 08b0dh                          ; eb f2
    mov dx, 08900h                            ; ba 00 89
    mov ax, 00cb2h                            ; b8 b2 0c
    jmp short 08b0dh                          ; eb ea
    or ah, 00ah                               ; 80 cc 0a
    mov word [bp+014h], ax                    ; 89 46 14
    mov word [bp+01ah], dx                    ; 89 56 1a
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn                                      ; c3
    mov word [bp+014h], 00102h                ; c7 46 14 02 01
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn                                      ; c3
    or ah, 080h                               ; 80 cc 80
    jmp short 08b26h                          ; eb ea
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 67 8d
    push word [bp+00eh]                       ; ff 76 0e
    push word [bp+014h]                       ; ff 76 14
    push 00cbbh                               ; 68 bb 0c
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 94 8d
    add sp, strict byte 00008h                ; 83 c4 08
    mov ax, word [bp+014h]                    ; 8b 46 14
    xor ah, ah                                ; 30 e4
    or ah, 00ch                               ; 80 cc 0c
    mov word [bp+014h], ax                    ; 89 46 14
    or byte [bp+01ah], 001h                   ; 80 4e 1a 01
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn                                      ; c3
pci16_select_reg_:                           ; 0xf8b6a LB 0x21
    push bx                                   ; 53
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    and dl, 0fch                              ; 80 e2 fc
    mov bx, dx                                ; 89 d3
    mov dx, 00cf8h                            ; ba f8 0c
    movzx eax, ax                             ; 66 0f b7 c0
    sal eax, 008h                             ; 66 c1 e0 08
    or eax, strict dword 080000000h           ; 66 0d 00 00 00 80
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3
    out DX, eax                               ; 66 ef
    pop bp                                    ; 5d
    pop bx                                    ; 5b
    retn                                      ; c3
pci16_find_device_:                          ; 0xf8b8b LB 0xf2
    push si                                   ; 56
    push di                                   ; 57
    enter 0000ch, 000h                        ; c8 0c 00 00
    push ax                                   ; 50
    push dx                                   ; 52
    mov si, bx                                ; 89 de
    mov di, cx                                ; 89 cf
    test cx, cx                               ; 85 c9
    xor bx, bx                                ; 31 db
    mov byte [bp-004h], 000h                  ; c6 46 fc 00
    test bl, 007h                             ; f6 c3 07
    jne short 08bd1h                          ; 75 2d
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, bx                                ; 89 d8
    call 08b6ah                               ; e8 be ff
    mov dx, 00cfeh                            ; ba fe 0c
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov byte [bp-002h], al                    ; 88 46 fe
    cmp AL, strict byte 0ffh                  ; 3c ff
    jne short 08bbfh                          ; 75 06
    add bx, strict byte 00008h                ; 83 c3 08
    jmp near 08c52h                           ; e9 93 00
    test byte [bp-002h], 080h                 ; f6 46 fe 80
    je short 08bcch                           ; 74 07
    mov word [bp-006h], strict word 00001h    ; c7 46 fa 01 00
    jmp short 08bd1h                          ; eb 05
    mov word [bp-006h], strict word 00008h    ; c7 46 fa 08 00
    mov al, byte [bp-002h]                    ; 8a 46 fe
    and AL, strict byte 007h                  ; 24 07
    cmp AL, strict byte 001h                  ; 3c 01
    jne short 08bf9h                          ; 75 1f
    mov ax, bx                                ; 89 d8
    shr ax, 008h                              ; c1 e8 08
    test ax, ax                               ; 85 c0
    jne short 08bf9h                          ; 75 16
    mov dx, strict word 0001ah                ; ba 1a 00
    mov ax, bx                                ; 89 d8
    call 08b6ah                               ; e8 7f ff
    mov dx, 00cfeh                            ; ba fe 0c
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp al, byte [bp-004h]                    ; 3a 46 fc
    jbe short 08bf9h                          ; 76 03
    mov byte [bp-004h], al                    ; 88 46 fc
    test di, di                               ; 85 ff
    je short 08c02h                           ; 74 05
    mov dx, strict word 00008h                ; ba 08 00
    jmp short 08c04h                          ; eb 02
    xor dx, dx                                ; 31 d2
    mov ax, bx                                ; 89 d8
    call 08b6ah                               ; e8 61 ff
    mov dx, 00cfch                            ; ba fc 0c
    in eax, DX                                ; 66 ed
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0
    shr eax, 010h                             ; 66 c1 e8 10
    xchg dx, ax                               ; 92
    mov word [bp-00ah], ax                    ; 89 46 f6
    mov word [bp-008h], dx                    ; 89 56 f8
    mov word [bp-00ch], strict word 00000h    ; c7 46 f4 00 00
    test di, di                               ; 85 ff
    je short 08c33h                           ; 74 0f
    mov cx, strict word 00008h                ; b9 08 00
    shr dx, 1                                 ; d1 ea
    rcr ax, 1                                 ; d1 d8
    loop 08c27h                               ; e2 fa
    mov word [bp-00ah], ax                    ; 89 46 f6
    mov word [bp-008h], dx                    ; 89 56 f8
    mov ax, word [bp-008h]                    ; 8b 46 f8
    cmp ax, word [bp-010h]                    ; 3b 46 f0
    jne short 08c43h                          ; 75 08
    mov ax, word [bp-00ah]                    ; 8b 46 f6
    cmp ax, word [bp-00eh]                    ; 3b 46 f2
    je short 08c49h                           ; 74 06
    cmp word [bp-00ch], strict byte 00000h    ; 83 7e f4 00
    je short 08c4fh                           ; 74 06
    dec si                                    ; 4e
    cmp si, strict byte 0ffffh                ; 83 fe ff
    je short 08c61h                           ; 74 12
    add bx, word [bp-006h]                    ; 03 5e fa
    mov dx, bx                                ; 89 da
    shr dx, 008h                              ; c1 ea 08
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc
    cmp dx, ax                                ; 39 c2
    jbe near 08b9fh                           ; 0f 86 3e ff
    cmp si, strict byte 0ffffh                ; 83 fe ff
    jne short 08c6ah                          ; 75 04
    mov ax, bx                                ; 89 d8
    jmp short 08c6dh                          ; eb 03
    mov ax, strict word 0ffffh                ; b8 ff ff
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
    pop bp                                    ; 5d
    lea si, [bx-073h]                         ; 8d 77 8d
    mov cl, byte [di-07261h]                  ; 8a 8d 9f 8d
    mov DL, strict byte 08dh                  ; b2 8d
    db  0c4h
    db  08dh
_pci16_function:                             ; 0xf8c7d LB 0x1bb
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    and word [bp+024h], 000ffh                ; 81 66 24 ff 00
    and word [bp+030h], strict byte 0fffeh    ; 83 66 30 fe
    mov bx, word [bp+024h]                    ; 8b 5e 24
    xor bh, bh                                ; 30 ff
    mov ax, word [bp+024h]                    ; 8b 46 24
    xor ah, ah                                ; 30 e4
    cmp bx, strict byte 00003h                ; 83 fb 03
    jc short 08cb4h                           ; 72 1a
    jbe short 08d0ch                          ; 76 70
    cmp bx, strict byte 0000eh                ; 83 fb 0e
    je near 08dd8h                            ; 0f 84 35 01
    cmp bx, strict byte 00008h                ; 83 fb 08
    jc near 08e07h                            ; 0f 82 5d 01
    cmp bx, strict byte 0000dh                ; 83 fb 0d
    jbe near 08d31h                           ; 0f 86 80 00
    jmp near 08e07h                           ; e9 53 01
    cmp bx, strict byte 00002h                ; 83 fb 02
    je short 08cdch                           ; 74 23
    cmp bx, strict byte 00001h                ; 83 fb 01
    jne near 08e07h                           ; 0f 85 47 01
    mov word [bp+024h], strict word 00001h    ; c7 46 24 01 00
    mov word [bp+018h], 00210h                ; c7 46 18 10 02
    mov word [bp+020h], strict word 00000h    ; c7 46 20 00 00
    mov word [bp+01ch], 04350h                ; c7 46 1c 50 43
    mov word [bp+01eh], 02049h                ; c7 46 1e 49 20
    jmp near 08e34h                           ; e9 58 01
    cmp word [bp+01ch], strict byte 0ffffh    ; 83 7e 1c ff
    jne short 08ce8h                          ; 75 06
    or ah, 083h                               ; 80 cc 83
    jmp near 08e2dh                           ; e9 45 01
    mov bx, word [bp+00ch]                    ; 8b 5e 0c
    mov dx, word [bp+020h]                    ; 8b 56 20
    mov ax, word [bp+01ch]                    ; 8b 46 1c
    xor cx, cx                                ; 31 c9
    call 08b8bh                               ; e8 95 fe
    cmp ax, strict word 0ffffh                ; 3d ff ff
    jne short 08d06h                          ; 75 0b
    mov ax, word [bp+024h]                    ; 8b 46 24
    xor ah, ah                                ; 30 e4
    or ah, 086h                               ; 80 cc 86
    jmp near 08e2dh                           ; e9 27 01
    mov word [bp+018h], ax                    ; 89 46 18
    jmp near 08e34h                           ; e9 28 01
    mov bx, word [bp+00ch]                    ; 8b 5e 0c
    mov ax, word [bp+020h]                    ; 8b 46 20
    mov dx, word [bp+022h]                    ; 8b 56 22
    mov cx, strict word 00001h                ; b9 01 00
    call 08b8bh                               ; e8 70 fe
    cmp ax, strict word 0ffffh                ; 3d ff ff
    jne short 08d2bh                          ; 75 0b
    mov ax, word [bp+024h]                    ; 8b 46 24
    xor ah, ah                                ; 30 e4
    or ah, 086h                               ; 80 cc 86
    jmp near 08e2dh                           ; e9 02 01
    mov word [bp+018h], ax                    ; 89 46 18
    jmp near 08e34h                           ; e9 03 01
    cmp word [bp+008h], 00100h                ; 81 7e 08 00 01
    jc short 08d3eh                           ; 72 06
    or ah, 087h                               ; 80 cc 87
    jmp near 08e2dh                           ; e9 ef 00
    mov dx, word [bp+008h]                    ; 8b 56 08
    mov ax, word [bp+018h]                    ; 8b 46 18
    call 08b6ah                               ; e8 23 fe
    mov bx, word [bp+024h]                    ; 8b 5e 24
    xor bh, bh                                ; 30 ff
    sub bx, strict byte 00008h                ; 83 eb 08
    cmp bx, strict byte 00005h                ; 83 fb 05
    jnbe near 08e34h                          ; 0f 87 de 00
    add bx, bx                                ; 01 db
    jmp word [cs:bx-0738fh]                   ; 2e ff a7 71 8c
    mov bx, word [bp+020h]                    ; 8b 5e 20
    xor bl, bl                                ; 30 db
    mov dx, word [bp+008h]                    ; 8b 56 08
    and dx, strict byte 00003h                ; 83 e2 03
    add dx, 00cfch                            ; 81 c2 fc 0c
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    or bx, ax                                 ; 09 c3
    mov word [bp+020h], bx                    ; 89 5e 20
    jmp near 08e34h                           ; e9 bd 00
    mov dx, word [bp+008h]                    ; 8b 56 08
    xor dh, dh                                ; 30 f6
    and dl, 002h                              ; 80 e2 02
    add dx, 00cfch                            ; 81 c2 fc 0c
    in ax, DX                                 ; ed
    mov word [bp+020h], ax                    ; 89 46 20
    jmp near 08e34h                           ; e9 aa 00
    mov dx, 00cfch                            ; ba fc 0c
    in eax, DX                                ; 66 ed
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0
    shr eax, 010h                             ; 66 c1 e8 10
    xchg dx, ax                               ; 92
    mov word [bp+020h], ax                    ; 89 46 20
    mov word [bp+022h], dx                    ; 89 56 22
    jmp near 08e34h                           ; e9 95 00
    mov ax, word [bp+020h]                    ; 8b 46 20
    mov dx, word [bp+008h]                    ; 8b 56 08
    xor dh, dh                                ; 30 f6
    and dl, 003h                              ; 80 e2 03
    add dx, 00cfch                            ; 81 c2 fc 0c
    out DX, AL                                ; ee
    jmp near 08e34h                           ; e9 82 00
    mov ax, word [bp+020h]                    ; 8b 46 20
    mov dx, word [bp+008h]                    ; 8b 56 08
    xor dh, dh                                ; 30 f6
    and dl, 002h                              ; 80 e2 02
    add dx, 00cfch                            ; 81 c2 fc 0c
    out DX, ax                                ; ef
    jmp short 08e34h                          ; eb 70
    mov ax, word [bp+020h]                    ; 8b 46 20
    mov cx, word [bp+022h]                    ; 8b 4e 22
    mov dx, 00cfch                            ; ba fc 0c
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    jmp short 08e34h                          ; eb 5c
    mov bx, word [bp+008h]                    ; 8b 5e 08
    mov es, [bp+028h]                         ; 8e 46 28
    mov cx, word [word 00000h]                ; 8b 0e 00 00
    cmp cx, word [es:bx]                      ; 26 3b 0f
    jbe short 08df1h                          ; 76 0a
    mov ax, word [bp+024h]                    ; 8b 46 24
    xor ah, ah                                ; 30 e4
    or ah, 089h                               ; 80 cc 89
    jmp short 08e2dh                          ; eb 3c
    les di, [es:bx+002h]                      ; 26 c4 7f 02
    mov si, 0f2c0h                            ; be c0 f2
    mov dx, ds                                ; 8c da
    push DS                                   ; 1e
    mov ds, dx                                ; 8e da
    rep movsb                                 ; f3 a4
    pop DS                                    ; 1f
    mov word [bp+018h], 00a00h                ; c7 46 18 00 0a
    jmp short 08e34h                          ; eb 2d
    mov bx, 00d32h                            ; bb 32 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018aeh                               ; e8 9c 8a
    mov ax, word [bp+018h]                    ; 8b 46 18
    push ax                                   ; 50
    mov ax, word [bp+024h]                    ; 8b 46 24
    push ax                                   ; 50
    push 00ceeh                               ; 68 ee 0c
    push strict byte 00004h                   ; 6a 04
    call 018e9h                               ; e8 c7 8a
    add sp, strict byte 00008h                ; 83 c4 08
    mov ax, word [bp+024h]                    ; 8b 46 24
    xor ah, ah                                ; 30 e4
    or ah, 081h                               ; 80 cc 81
    mov word [bp+024h], ax                    ; 89 46 24
    or word [bp+030h], strict byte 00001h     ; 83 4e 30 01
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
pci_find_classcode_:                         ; 0xf8e38 LB 0x28
    push bx                                   ; 53
    push cx                                   ; 51
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov cx, dx                                ; 89 d1
    xor si, si                                ; 31 f6
    mov dx, ax                                ; 89 c2
    mov ax, 0b103h                            ; b8 03 b1
    sal ecx, 010h                             ; 66 c1 e1 10
    db  08bh, 0cah
    ; mov cx, dx                                ; 8b ca
    int 01ah                                  ; cd 1a
    cmp ah, 000h                              ; 80 fc 00
    je near 08e59h                            ; 0f 84 03 00
    mov bx, strict word 0ffffh                ; bb ff ff
    mov ax, bx                                ; 89 d8
    pop bp                                    ; 5d
    pop si                                    ; 5e
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
pci_read_config_byte_:                       ; 0xf8e60 LB 0x21
    push cx                                   ; 51
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    movzx di, bl                              ; 0f b6 fb
    movzx bx, al                              ; 0f b6 d8
    sal bx, 008h                              ; c1 e3 08
    movzx ax, dl                              ; 0f b6 c2
    or bx, ax                                 ; 09 c3
    mov ax, 0b108h                            ; b8 08 b1
    int 01ah                                  ; cd 1a
    movzx ax, cl                              ; 0f b6 c1
    xor dx, dx                                ; 31 d2
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop cx                                    ; 59
    retn                                      ; c3
pci_read_config_word_:                       ; 0xf8e81 LB 0x1f
    push cx                                   ; 51
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    movzx di, bl                              ; 0f b6 fb
    xor ah, ah                                ; 30 e4
    sal ax, 008h                              ; c1 e0 08
    movzx bx, dl                              ; 0f b6 da
    or bx, ax                                 ; 09 c3
    mov ax, 0b109h                            ; b8 09 b1
    int 01ah                                  ; cd 1a
    mov ax, cx                                ; 89 c8
    xor dx, dx                                ; 31 d2
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop cx                                    ; 59
    retn                                      ; c3
pci_read_config_dword_:                      ; 0xf8ea0 LB 0x24
    push cx                                   ; 51
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    movzx di, bl                              ; 0f b6 fb
    movzx bx, al                              ; 0f b6 d8
    sal bx, 008h                              ; c1 e3 08
    movzx ax, dl                              ; 0f b6 c2
    or bx, ax                                 ; 09 c3
    mov ax, 0b10ah                            ; b8 0a b1
    int 01ah                                  ; cd 1a
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    shr ecx, 010h                             ; 66 c1 e9 10
    mov dx, cx                                ; 89 ca
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop cx                                    ; 59
    retn                                      ; c3
vds_is_present_:                             ; 0xf8ec4 LB 0x1d
    push bx                                   ; 53
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, strict word 0007bh                ; bb 7b 00
    mov ax, strict word 00040h                ; b8 40 00
    mov es, ax                                ; 8e c0
    test byte [es:bx], 020h                   ; 26 f6 07 20
    je short 08edch                           ; 74 06
    mov ax, strict word 00001h                ; b8 01 00
    pop bp                                    ; 5d
    pop bx                                    ; 5b
    retn                                      ; c3
    xor ax, ax                                ; 31 c0
    pop bp                                    ; 5d
    pop bx                                    ; 5b
    retn                                      ; c3
vds_real_to_lin_:                            ; 0xf8ee1 LB 0x1e
    push bx                                   ; 53
    push cx                                   ; 51
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, ax                                ; 89 c3
    mov ax, dx                                ; 89 d0
    xor dx, dx                                ; 31 d2
    mov cx, strict word 00004h                ; b9 04 00
    sal ax, 1                                 ; d1 e0
    rcl dx, 1                                 ; d1 d2
    loop 08eefh                               ; e2 fa
    xor cx, cx                                ; 31 c9
    add ax, bx                                ; 01 d8
    adc dx, cx                                ; 11 ca
    pop bp                                    ; 5d
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
vds_build_sg_list_:                          ; 0xf8eff LB 0x76
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov di, ax                                ; 89 c7
    mov si, dx                                ; 89 d6
    mov ax, bx                                ; 89 d8
    mov dx, cx                                ; 89 ca
    mov bx, word [bp+008h]                    ; 8b 5e 08
    mov es, si                                ; 8e c6
    mov word [es:di], bx                      ; 26 89 1d
    mov bx, word [bp+00ah]                    ; 8b 5e 0a
    mov word [es:di+002h], bx                 ; 26 89 5d 02
    call 08ee1h                               ; e8 c3 ff
    mov es, si                                ; 8e c6
    mov word [es:di+004h], ax                 ; 26 89 45 04
    mov word [es:di+006h], dx                 ; 26 89 55 06
    mov word [es:di+008h], strict word 00000h ; 26 c7 45 08 00 00
    call 08ec4h                               ; e8 93 ff
    test ax, ax                               ; 85 c0
    je short 08f48h                           ; 74 13
    mov es, si                                ; 8e c6
    mov ax, 08105h                            ; b8 05 81
    mov dx, strict word 00000h                ; ba 00 00
    int 04bh                                  ; cd 4b
    jc near 08f45h                            ; 0f 82 02 00
    db  032h, 0c0h
    ; xor al, al                                ; 32 c0
    cbw                                       ; 98
    jmp short 08f6fh                          ; eb 27
    mov es, si                                ; 8e c6
    mov word [es:di+00eh], strict word 00001h ; 26 c7 45 0e 01 00
    mov dx, word [es:di+004h]                 ; 26 8b 55 04
    mov ax, word [es:di+006h]                 ; 26 8b 45 06
    mov word [es:di+010h], dx                 ; 26 89 55 10
    mov word [es:di+012h], ax                 ; 26 89 45 12
    mov ax, word [bp+008h]                    ; 8b 46 08
    mov word [es:di+014h], ax                 ; 26 89 45 14
    mov ax, bx                                ; 89 d8
    mov word [es:di+016h], bx                 ; 26 89 5d 16
    xor ax, bx                                ; 31 d8
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn 00004h                               ; c2 04 00
vds_free_sg_list_:                           ; 0xf8f75 LB 0x3b
    push bx                                   ; 53
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, ax                                ; 89 c3
    call 08ec4h                               ; e8 45 ff
    test ax, ax                               ; 85 c0
    je short 08f96h                           ; 74 13
    mov di, bx                                ; 89 df
    mov es, dx                                ; 8e c2
    mov ax, 08106h                            ; b8 06 81
    mov dx, strict word 00000h                ; ba 00 00
    int 04bh                                  ; cd 4b
    jc near 08f95h                            ; 0f 82 02 00
    db  032h, 0c0h
    ; xor al, al                                ; 32 c0
    cbw                                       ; 98
    mov es, dx                                ; 8e c2
    mov word [es:bx+00eh], strict word 00000h ; 26 c7 47 0e 00 00
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop bx                                    ; 5b
    retn                                      ; c3
    times 0xe db 0
__U4D:                                       ; 0xf8fb0 LB 0x39
    pushfw                                    ; 9c
    push eax                                  ; 66 50
    push edx                                  ; 66 52
    push ecx                                  ; 66 51
    rol eax, 010h                             ; 66 c1 c0 10
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2
    ror eax, 010h                             ; 66 c1 c8 10
    db  066h, 033h, 0d2h
    ; xor edx, edx                              ; 66 33 d2
    shr ecx, 010h                             ; 66 c1 e9 10
    db  08bh, 0cbh
    ; mov cx, bx                                ; 8b cb
    div ecx                                   ; 66 f7 f1
    db  08bh, 0dah
    ; mov bx, dx                                ; 8b da
    pop ecx                                   ; 66 59
    shr edx, 010h                             ; 66 c1 ea 10
    db  08bh, 0cah
    ; mov cx, dx                                ; 8b ca
    pop edx                                   ; 66 5a
    ror eax, 010h                             ; 66 c1 c8 10
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0
    add sp, strict byte 00002h                ; 83 c4 02
    pop ax                                    ; 58
    rol eax, 010h                             ; 66 c1 c0 10
    popfw                                     ; 9d
    retn                                      ; c3
__U4M:                                       ; 0xf8fe9 LB 0x31
    pushfw                                    ; 9c
    push eax                                  ; 66 50
    push edx                                  ; 66 52
    push ecx                                  ; 66 51
    rol eax, 010h                             ; 66 c1 c0 10
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2
    ror eax, 010h                             ; 66 c1 c8 10
    db  066h, 033h, 0d2h
    ; xor edx, edx                              ; 66 33 d2
    shr ecx, 010h                             ; 66 c1 e9 10
    db  08bh, 0cbh
    ; mov cx, bx                                ; 8b cb
    mul ecx                                   ; 66 f7 e1
    pop ecx                                   ; 66 59
    pop edx                                   ; 66 5a
    ror eax, 010h                             ; 66 c1 c8 10
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0
    add sp, strict byte 00002h                ; 83 c4 02
    pop ax                                    ; 58
    rol eax, 010h                             ; 66 c1 c0 10
    popfw                                     ; 9d
    retn                                      ; c3
_fmemset_:                                   ; 0xf901a LB 0xd
    push di                                   ; 57
    mov es, dx                                ; 8e c2
    db  08bh, 0f8h
    ; mov di, ax                                ; 8b f8
    xchg al, bl                               ; 86 d8
    rep stosb                                 ; f3 aa
    xchg al, bl                               ; 86 d8
    pop di                                    ; 5f
    retn                                      ; c3
_fmemcpy_:                                   ; 0xf9027 LB 0x33
    push bp                                   ; 55
    db  08bh, 0ech
    ; mov bp, sp                                ; 8b ec
    push di                                   ; 57
    push DS                                   ; 1e
    push si                                   ; 56
    mov es, dx                                ; 8e c2
    db  08bh, 0f8h
    ; mov di, ax                                ; 8b f8
    mov ds, cx                                ; 8e d9
    db  08bh, 0f3h
    ; mov si, bx                                ; 8b f3
    mov cx, word [bp+004h]                    ; 8b 4e 04
    rep movsb                                 ; f3 a4
    pop si                                    ; 5e
    pop DS                                    ; 1f
    pop di                                    ; 5f
    leave                                     ; c9
    retn                                      ; c3
    add byte [bx+si-070h], dh                 ; 00 70 90
    jc short 08fd4h                           ; 72 90
    jbe short 08fd6h                          ; 76 90
    jbe short 08fd8h                          ; 76 90
    jbe short 08fdah                          ; 76 90
    js short 08fdch                           ; 78 90
    js short 08fdeh                           ; 78 90
    jp short 08fe0h                           ; 7a 90
    jle short 08fe2h                          ; 7e 90
    jle short 08fe4h                          ; 7e 90
    adc byte [bx+si-06f7bh], 087h             ; 80 90 85 90 87
    nop                                       ; 90
apm_worker:                                  ; 0xf905a LB 0x3a
    sti                                       ; fb
    push ax                                   ; 50
    db  032h, 0e4h
    ; xor ah, ah                                ; 32 e4
    sub AL, strict byte 004h                  ; 2c 04
    db  08bh, 0e8h
    ; mov bp, ax                                ; 8b e8
    sal bp, 1                                 ; d1 e5
    cmp AL, strict byte 00dh                  ; 3c 0d
    pop ax                                    ; 58
    mov AH, strict byte 053h                  ; b4 53
    jnc short 09090h                          ; 73 25
    jmp word [cs:bp-06fc0h]                   ; 2e ff a6 40 90
    jmp short 0908eh                          ; eb 1c
    sti                                       ; fb
    hlt                                       ; f4
    jmp short 0908eh                          ; eb 18
    jmp short 0908eh                          ; eb 16
    jmp short 09090h                          ; eb 16
    mov AH, strict byte 080h                  ; b4 80
    jmp short 09092h                          ; eb 14
    jmp short 09090h                          ; eb 10
    mov ax, 00102h                            ; b8 02 01
    jmp short 0908eh                          ; eb 09
    jmp short 0908eh                          ; eb 07
    mov BL, strict byte 000h                  ; b3 00
    mov cx, strict word 00000h                ; b9 00 00
    jmp short 0908eh                          ; eb 00
    clc                                       ; f8
    retn                                      ; c3
    mov AH, strict byte 009h                  ; b4 09
    stc                                       ; f9
    retn                                      ; c3
apm_pm16_entry:                              ; 0xf9094 LB 0x11
    mov AH, strict byte 002h                  ; b4 02
    push DS                                   ; 1e
    push bp                                   ; 55
    push CS                                   ; 0e
    pop bp                                    ; 5d
    add bp, strict byte 00008h                ; 83 c5 08
    mov ds, bp                                ; 8e dd
    call 0905ah                               ; e8 b8 ff
    pop bp                                    ; 5d
    pop DS                                    ; 1f
    retf                                      ; cb

  ; Padding 0x495b bytes at 0xf90a5
  times 18779 db 0

section BIOS32 progbits vstart=0xda00 align=1 ; size=0x3aa class=CODE group=AUTO
bios32_service:                              ; 0xfda00 LB 0x26
    pushfw                                    ; 9c
    cmp bl, 000h                              ; 80 fb 00
    jne short 0da22h                          ; 75 1c
    cmp ax, 05024h                            ; 3d 24 50
    inc bx                                    ; 43
    dec cx                                    ; 49
    mov AL, strict byte 080h                  ; b0 80
    jne short 0da20h                          ; 75 11
    mov bx, strict word 00000h                ; bb 00 00
    db  00fh
    add byte [bx+di-01000h], bh               ; 00 b9 00 f0
    add byte [bx+si], al                      ; 00 00
    mov dx, 0da26h                            ; ba 26 da
    add byte [bx+si], al                      ; 00 00
    db  032h, 0c0h
    ; xor al, al                                ; 32 c0
    popfw                                     ; 9d
    retf                                      ; cb
    mov AL, strict byte 081h                  ; b0 81
    jmp short 0da20h                          ; eb fa
pcibios32_entry:                             ; 0xfda26 LB 0x1a
    pushfw                                    ; 9c
    cld                                       ; fc
    push ES                                   ; 06
    pushaw                                    ; 60
    call 0db73h                               ; e8 46 01
    add byte [bx+si], al                      ; 00 00
    popaw                                     ; 61
    pop ES                                    ; 07
    popfw                                     ; 9d
    retf                                      ; cb
    times 0xd db 0
apm_pm32_entry:                              ; 0xfda40 LB 0x21
    push bp                                   ; 55
    mov ebp, cs                               ; 66 8c cd
    push ebp                                  ; 66 55
    mov bp, 0da5fh                            ; bd 5f da
    add byte [bx+si], al                      ; 00 00
    push ebp                                  ; 66 55
    push CS                                   ; 0e
    pop bp                                    ; 5d
    add bp, strict byte 00008h                ; 83 c5 08
    push ebp                                  ; 66 55
    mov bp, 09096h                            ; bd 96 90
    add byte [bx+si], al                      ; 00 00
    push ebp                                  ; 66 55
    mov AH, strict byte 003h                  ; b4 03
    db  066h, 0cbh
    ; retf                                      ; 66 cb
    pop bp                                    ; 5d
    retf                                      ; cb
pci32_select_reg_:                           ; 0xfda61 LB 0x1f
    push bx                                   ; 53
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    and dl, 0fch                              ; 80 e2 fc
    mov bx, dx                                ; 89 d3
    mov dx, 00cf8h                            ; ba f8 0c
    add byte [bx+si], al                      ; 00 00
    db  00fh, 0b7h, 0c0h
    ; movzx ax, ax                              ; 0f b7 c0
    sal ax, 008h                              ; c1 e0 08
    or ax, strict word 00000h                 ; 0d 00 00
    add byte [bx+si-03c76h], al               ; 00 80 8a c3
    out DX, ax                                ; ef
    pop bp                                    ; 5d
    pop bx                                    ; 5b
    retn                                      ; c3
pci32_find_device_:                          ; 0xfda80 LB 0xf5
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    enter 00014h, 000h                        ; c8 14 00 00
    push ax                                   ; 50
    mov cx, dx                                ; 89 d1
    mov si, bx                                ; 89 de
    test bx, bx                               ; 85 db
    xor bx, bx                                ; 31 db
    mov byte [di-004h], 000h                  ; c6 45 fc 00
    test bl, 007h                             ; f6 c3 07
    jne short 0dacfh                          ; 75 36
    db  00fh, 0b7h, 0c3h
    ; movzx ax, bx                              ; 0f b7 c3
    mov dx, strict word 0000eh                ; ba 0e 00
    add byte [bx+si], al                      ; 00 00
    call 0da5fh                               ; e8 bb ff
    db  0ffh
    db  0ffh
    mov dx, 00cfeh                            ; ba fe 0c
    add byte [bx+si], al                      ; 00 00
    db  02bh, 0c0h
    ; sub ax, ax                                ; 2b c0
    in AL, DX                                 ; ec
    mov byte [di-008h], al                    ; 88 45 f8
    cmp AL, strict byte 0ffh                  ; 3c ff
    jne short 0dabdh                          ; 75 08
    add bx, strict byte 00008h                ; 83 c3 08
    jmp near 0db45h                           ; e9 8a 00
    add byte [bx+si], al                      ; 00 00
    test byte [di-008h], 080h                 ; f6 45 f8 80
    je short 0dacah                           ; 74 07
    mov di, strict word 00001h                ; bf 01 00
    add byte [bx+si], al                      ; 00 00
    jmp short 0dacfh                          ; eb 05
    mov di, strict word 00008h                ; bf 08 00
    add byte [bx+si], al                      ; 00 00
    mov al, byte [di-008h]                    ; 8a 45 f8
    and AL, strict byte 007h                  ; 24 07
    cmp AL, strict byte 001h                  ; 3c 01
    jne short 0dafeh                          ; 75 26
    db  00fh, 0b7h, 0c3h
    ; movzx ax, bx                              ; 0f b7 c3
    mov dx, ax                                ; 89 c2
    sar dx, 008h                              ; c1 fa 08
    test dx, dx                               ; 85 d2
    jne short 0dafeh                          ; 75 1a
    mov dx, strict word 0001ah                ; ba 1a 00
    add byte [bx+si], al                      ; 00 00
    call 0da5fh                               ; e8 73 ff
    db  0ffh
    db  0ffh
    mov dx, 00cfeh                            ; ba fe 0c
    add byte [bx+si], al                      ; 00 00
    db  02bh, 0c0h
    ; sub ax, ax                                ; 2b c0
    in AL, DX                                 ; ec
    cmp al, byte [di-004h]                    ; 3a 45 fc
    jbe short 0dafeh                          ; 76 03
    mov byte [di-004h], al                    ; 88 45 fc
    test si, si                               ; 85 f6
    je short 0db09h                           ; 74 07
    mov ax, strict word 00008h                ; b8 08 00
    add byte [bx+si], al                      ; 00 00
    jmp short 0db0bh                          ; eb 02
    xor ax, ax                                ; 31 c0
    db  00fh, 0b7h, 0d0h
    ; movzx dx, ax                              ; 0f b7 d0
    db  00fh, 0b7h, 0c3h
    ; movzx ax, bx                              ; 0f b7 c3
    call 0da5fh                               ; e8 4b ff
    db  0ffh
    db  0ffh
    mov dx, 00cfch                            ; ba fc 0c
    add byte [bx+si], al                      ; 00 00
    in ax, DX                                 ; ed
    mov word [di-00ch], ax                    ; 89 45 f4
    mov word [di-014h], strict word 00000h    ; c7 45 ec 00 00
    add byte [bx+si], al                      ; 00 00
    test si, si                               ; 85 f6
    je short 0db30h                           ; 74 06
    shr ax, 008h                              ; c1 e8 08
    mov word [di-00ch], ax                    ; 89 45 f4
    mov ax, word [di-00ch]                    ; 8b 45 f4
    cmp ax, word [di-018h]                    ; 3b 45 e8
    je short 0db3eh                           ; 74 06
    cmp word [di-014h], strict byte 00000h    ; 83 7d ec 00
    je short 0db45h                           ; 74 07
    dec cx                                    ; 49
    cmp ecx, strict byte 0ffffffffh           ; 66 83 f9 ff
    je short 0db5dh                           ; 74 18
    add bx, di                                ; 01 fb
    db  00fh, 0b7h, 0c3h
    ; movzx ax, bx                              ; 0f b7 c3
    sar ax, 008h                              ; c1 f8 08
    mov word [di-010h], ax                    ; 89 45 f0
    movzx ax, byte [di-004h]                  ; 0f b6 45 fc
    cmp ax, word [di-010h]                    ; 3b 45 f0
    jnl near 0da92h                           ; 0f 8d 37 ff
    db  0ffh
    jmp word [bp-07dh]                        ; ff 66 83
    stc                                       ; f9
    push word [di+008h]                       ; ff 75 08
    db  00fh, 0b7h, 0c3h
    ; movzx ax, bx                              ; 0f b7 c3
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    retn                                      ; c3
    mov ax, strict word 0ffffh                ; b8 ff ff
    add byte [bx+si], al                      ; 00 00
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    retn                                      ; c3
_pci32_function:                             ; 0xfdb75 LB 0x235
    push bx                                   ; 53
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    and dword [di+030h], strict dword 0658100ffh ; 66 81 65 30 ff 00 81 65
    cmp dh, bh                                ; 38 fe
    inc word [bx+si]                          ; ff 00
    add byte [bp+di+03045h], cl               ; 00 8b 45 30
    xor ah, ah                                ; 30 e4
    cmp eax, strict dword 029720003h          ; 66 3d 03 00 72 29
    jbe near 0dc30h                           ; 0f 86 99 00
    add byte [bx+si], al                      ; 00 00
    cmp eax, strict dword 0840f000eh          ; 66 3d 0e 00 0f 84
    lodsw                                     ; ad
    add word [bx+si], ax                      ; 01 00
    add byte [bp+03dh], ah                    ; 00 66 3d
    or byte [bx+si], al                       ; 08 00
    jc near 0dd93h                            ; 0f 82 e8 01
    add byte [bx+si], al                      ; 00 00
    cmp eax, strict dword 0860f000dh          ; 66 3d 0d 00 0f 86
    test AL, strict byte 000h                 ; a8 00
    add byte [bx+si], al                      ; 00 00
    jmp near 0dd93h                           ; e9 d9 01
    add byte [bx+si], al                      ; 00 00
    cmp eax, strict dword 028740002h          ; 66 3d 02 00 74 28
    cmp eax, strict dword 0850f0001h          ; 66 3d 01 00 0f 85
    leave                                     ; c9
    add word [bx+si], ax                      ; 01 00
    add byte [bp-039h], ah                    ; 00 66 c7
    inc bp                                    ; 45
    xor byte [bx+di], al                      ; 30 01
    add byte [bp-039h], ah                    ; 00 66 c7
    inc bp                                    ; 45
    and AL, strict byte 010h                  ; 24 10
    add dh, byte [bx+di]                      ; 02 31
    sal byte [bp-077h], 045h                  ; c0 66 89 45
    sub AL, strict byte 0c7h                  ; 2c c7
    inc bp                                    ; 45
    sub byte [bx+si+043h], dl                 ; 28 50 43
    dec cx                                    ; 49
    and byte [di+05fh], bl                    ; 20 5d 5f
    pop si                                    ; 5e
    pop bx                                    ; 5b
    retn                                      ; c3
    cmp dword [di+028h], strict byte 0ffffffffh ; 66 83 7d 28 ff
    jne short 0dbfeh                          ; 75 0d
    mov ax, word [di+030h]                    ; 8b 45 30
    xor ah, ah                                ; 30 e4
    or ah, 083h                               ; 80 cc 83
    jmp near 0dd9bh                           ; e9 9f 01
    add byte [bx+si], al                      ; 00 00
    xor bx, bx                                ; 31 db
    db  00fh, 0b7h, 055h, 018h
    ; movzx dx, word [di+018h]                  ; 0f b7 55 18
    db  00fh, 0b7h, 04dh, 02ch
    ; movzx cx, word [di+02ch]                  ; 0f b7 4d 2c
    sal cx, 010h                              ; c1 e1 10
    db  00fh, 0b7h, 045h, 028h
    ; movzx ax, word [di+028h]                  ; 0f b7 45 28
    or ax, cx                                 ; 09 c8
    call 0da7eh                               ; e8 6a fe
    db  0ffh
    jmp word [bp+03dh]                        ; ff 66 3d
    db  0ffh
    push word [di+00dh]                       ; ff 75 0d
    mov ax, word [di+030h]                    ; 8b 45 30
    xor ah, ah                                ; 30 e4
    or ah, 086h                               ; 80 cc 86
    jmp near 0dd9bh                           ; e9 74 01
    add byte [bx+si], al                      ; 00 00
    mov dword [di+024h], eax                  ; 66 89 45 24
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bx                                    ; 5b
    retn                                      ; c3
    db  00fh, 0b7h, 055h, 018h
    ; movzx dx, word [di+018h]                  ; 0f b7 55 18
    mov ax, word [di+02ch]                    ; 8b 45 2c
    mov bx, strict word 00001h                ; bb 01 00
    add byte [bx+si], al                      ; 00 00
    call 0da7eh                               ; e8 3d fe
    db  0ffh
    jmp word [bp+03dh]                        ; ff 66 3d
    db  0ffh
    push word [di+00dh]                       ; ff 75 0d
    mov ax, word [di+030h]                    ; 8b 45 30
    xor ah, ah                                ; 30 e4
    or ah, 086h                               ; 80 cc 86
    jmp near 0dd9bh                           ; e9 47 01
    add byte [bx+si], al                      ; 00 00
    mov dword [di+024h], eax                  ; 66 89 45 24
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bx                                    ; 5b
    retn                                      ; c3
    cmp dword [di+014h], strict dword 00d720100h ; 66 81 7d 14 00 01 72 0d
    mov ax, word [di+030h]                    ; 8b 45 30
    xor ah, ah                                ; 30 e4
    or ah, 087h                               ; 80 cc 87
    jmp near 0dd9bh                           ; e9 29 01
    add byte [bx+si], al                      ; 00 00
    db  00fh, 0b7h, 055h, 014h
    ; movzx dx, word [di+014h]                  ; 0f b7 55 14
    db  00fh, 0b7h, 045h, 024h
    ; movzx ax, word [di+024h]                  ; 0f b7 45 24
    call 0da5fh                               ; e8 e0 fd
    db  0ffh
    dec word [bp+di+03045h]                   ; ff 8b 45 30
    xor ah, ah                                ; 30 e4
    cmp eax, strict dword 02172000ah          ; 66 3d 0a 00 72 21
    jbe short 0dcfeh                          ; 76 70
    cmp eax, strict dword 0840f000dh          ; 66 3d 0d 00 0f 84
    stosb                                     ; aa
    add byte [bx+si], al                      ; 00 00
    add byte [bp+03dh], ah                    ; 00 66 3d
    or AL, strict byte 000h                   ; 0c 00
    je near 0dd24h                            ; 0f 84 84 00
    add byte [bx+si], al                      ; 00 00
    cmp eax, strict dword 06474000bh          ; 66 3d 0b 00 74 64
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bx                                    ; 5b
    retn                                      ; c3
    cmp eax, strict dword 02e740009h          ; 66 3d 09 00 74 2e
    cmp eax, strict dword 005740008h          ; 66 3d 08 00 74 05
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bx                                    ; 5b
    retn                                      ; c3
    mov bx, word [di+02ch]                    ; 8b 5d 2c
    xor bl, bl                                ; 30 db
    mov ax, word [di+014h]                    ; 8b 45 14
    xor ah, ah                                ; 30 e4
    and AL, strict byte 003h                  ; 24 03
    db  00fh, 0b7h, 0d0h
    ; movzx dx, ax                              ; 0f b7 d0
    add dx, 00cfch                            ; 81 c2 fc 0c
    add byte [bx+si], al                      ; 00 00
    db  02bh, 0c0h
    ; sub ax, ax                                ; 2b c0
    in AL, DX                                 ; ec
    or bx, ax                                 ; 09 c3
    mov dword [di+02ch], ebx                  ; 66 89 5d 2c
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bx                                    ; 5b
    retn                                      ; c3
    mov ax, word [di+014h]                    ; 8b 45 14
    xor ah, ah                                ; 30 e4
    and AL, strict byte 002h                  ; 24 02
    db  00fh, 0b7h, 0d0h
    ; movzx dx, ax                              ; 0f b7 d0
    add dx, 00cfch                            ; 81 c2 fc 0c
    add byte [bx+si], al                      ; 00 00
    db  02bh, 0c0h
    ; sub ax, ax                                ; 2b c0
    in eax, DX                                ; 66 ed
    mov dword [di+02ch], eax                  ; 66 89 45 2c
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bx                                    ; 5b
    retn                                      ; c3
    mov dx, 00cfch                            ; ba fc 0c
    add byte [bx+si], al                      ; 00 00
    in ax, DX                                 ; ed
    mov word [di+02ch], ax                    ; 89 45 2c
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bx                                    ; 5b
    retn                                      ; c3
    mov ax, word [di+02ch]                    ; 8b 45 2c
    mov dx, word [di+014h]                    ; 8b 55 14
    xor dh, dh                                ; 30 f6
    and dl, 003h                              ; 80 e2 03
    db  00fh, 0b7h, 0d2h
    ; movzx dx, dx                              ; 0f b7 d2
    add dx, 00cfch                            ; 81 c2 fc 0c
    add byte [bx+si], al                      ; 00 00
    out DX, AL                                ; ee
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bx                                    ; 5b
    retn                                      ; c3
    db  00fh, 0b7h, 045h, 02ch
    ; movzx ax, word [di+02ch]                  ; 0f b7 45 2c
    mov dx, word [di+014h]                    ; 8b 55 14
    xor dh, dh                                ; 30 f6
    and dl, 002h                              ; 80 e2 02
    db  00fh, 0b7h, 0d2h
    ; movzx dx, dx                              ; 0f b7 d2
    add dx, 00cfch                            ; 81 c2 fc 0c
    add byte [bx+si], al                      ; 00 00
    out DX, eax                               ; 66 ef
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bx                                    ; 5b
    retn                                      ; c3
    mov ax, word [di+02ch]                    ; 8b 45 2c
    mov dx, 00cfch                            ; ba fc 0c
    add byte [bx+si], al                      ; 00 00
    out DX, ax                                ; ef
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bx                                    ; 5b
    retn                                      ; c3
    db  00fh, 0b7h, 045h, 014h
    ; movzx ax, word [di+014h]                  ; 0f b7 45 14
    mov es, [di+034h]                         ; 8e 45 34
    mov ecx, dword [di]                       ; 66 8b 0d
    add byte [bx+si], al                      ; 00 00
    add byte [bx+si], al                      ; 00 00
    db  066h, 026h, 03bh, 008h
    ; cmp ecx, dword [es:bx+si]                 ; 66 26 3b 08
    jbe short 0dd6eh                          ; 76 0a
    mov ax, word [di+030h]                    ; 8b 45 30
    xor ah, ah                                ; 30 e4
    or ah, 089h                               ; 80 cc 89
    jmp short 0dd9dh                          ; eb 2f
    db  00fh, 0b7h, 0c9h
    ; movzx cx, cx                              ; 0f b7 c9
    db  066h, 026h, 08bh, 058h, 006h
    ; mov ebx, dword [es:bx+si+006h]            ; 66 26 8b 58 06
    mov di, word [es:bx+si+002h]              ; 26 8b 78 02
    mov dx, ds                                ; 8c da
    mov si, 0f2c0h                            ; be c0 f2
    add byte [bx+si], al                      ; 00 00
    mov es, bx                                ; 8e c3
    push DS                                   ; 1e
    db  066h, 08eh, 0dah
    ; mov ds, edx                               ; 66 8e da
    rep movsb                                 ; f3 a4
    pop DS                                    ; 1f
    mov dword [di+024h], strict dword 05f5d0a00h ; 66 c7 45 24 00 0a 5d 5f
    pop si                                    ; 5e
    pop bx                                    ; 5b
    retn                                      ; c3
    mov ax, word [di+030h]                    ; 8b 45 30
    xor ah, ah                                ; 30 e4
    or ah, 081h                               ; 80 cc 81
    mov dword [di+030h], eax                  ; 66 89 45 30
    or word [di+038h], strict byte 00001h     ; 83 4d 38 01
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bx                                    ; 5b
    retn                                      ; c3

  ; Padding 0x2 bytes at 0xfddaa
  times 2 db 0

section BIOS32CONST progbits vstart=0xddac align=1 ; size=0x0 class=FAR_DATA group=BIOS32_GROUP

section BIOS32CONST2 progbits vstart=0xddac align=1 ; size=0x0 class=FAR_DATA group=BIOS32_GROUP

section BIOS32_DATA progbits vstart=0xddac align=1 ; size=0x0 class=FAR_DATA group=BIOS32_GROUP

  ; Padding 0x254 bytes at 0xfddac
  times 596 db 0

section BIOSSEG progbits vstart=0xe000 align=1 ; size=0x2000 class=CODE group=AUTO
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 058h, 04dh
eoi_jmp_post:                                ; 0xfe030 LB 0xb
    call 0e03bh                               ; e8 08 00
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    mov ds, ax                                ; 8e d8
    jmp far [00467h]                          ; ff 2e 67 04
eoi_both_pics:                               ; 0xfe03b LB 0x4
    mov AL, strict byte 020h                  ; b0 20
    out strict byte 0a0h, AL                  ; e6 a0
eoi_master_pic:                              ; 0xfe03f LB 0x5
    mov AL, strict byte 020h                  ; b0 20
    out strict byte 020h, AL                  ; e6 20
    retn                                      ; c3
set_int_vects:                               ; 0xfe044 LB 0x17
    mov word [bx], ax                         ; 89 07
    mov word [bx+002h], dx                    ; 89 57 02
    add bx, strict byte 00004h                ; 83 c3 04
    loop 0e044h                               ; e2 f6
    retn                                      ; c3
    times 0xa db 0
    db  'XM'
post:                                        ; 0xfe05b LB 0x3f
    cli                                       ; fa
    smsw ax                                   ; 0f 01 e0
    test ax, strict word 00001h               ; a9 01 00
    je short 0e068h                           ; 74 04
    out strict byte 092h, AL                  ; e6 92
    jmp short 0e066h                          ; eb fe
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    out strict byte 00dh, AL                  ; e6 0d
    out strict byte 0dah, AL                  ; e6 da
    mov AL, strict byte 0c0h                  ; b0 c0
    out strict byte 0d6h, AL                  ; e6 d6
    mov AL, strict byte 000h                  ; b0 00
    out strict byte 0d4h, AL                  ; e6 d4
    mov AL, strict byte 00fh                  ; b0 0f
    out strict byte 070h, AL                  ; e6 70
    in AL, strict byte 071h                   ; e4 71
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8
    mov AL, strict byte 00fh                  ; b0 0f
    out strict byte 070h, AL                  ; e6 70
    mov AL, strict byte 000h                  ; b0 00
    out strict byte 071h, AL                  ; e6 71
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3
    cmp AL, strict byte 000h                  ; 3c 00
    je short 0e09ah                           ; 74 0e
    cmp AL, strict byte 00dh                  ; 3c 0d
    jnc short 0e09ah                          ; 73 0a
    cmp AL, strict byte 009h                  ; 3c 09
    je short 0e09ah                           ; 74 06
    cmp AL, strict byte 005h                  ; 3c 05
    je short 0e030h                           ; 74 98
    jmp short 0e09ah                          ; eb 00
normal_post:                                 ; 0xfe09a LB 0x229
    mov ax, 07800h                            ; b8 00 78
    db  08bh, 0e0h
    ; mov sp, ax                                ; 8b e0
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    mov ds, ax                                ; 8e d8
    mov ss, ax                                ; 8e d0
    mov es, ax                                ; 8e c0
    db  033h, 0ffh
    ; xor di, di                                ; 33 ff
    cld                                       ; fc
    mov cx, 00239h                            ; b9 39 02
    rep stosw                                 ; f3 ab
    inc di                                    ; 47
    inc di                                    ; 47
    mov cx, 005c6h                            ; b9 c6 05
    rep stosw                                 ; f3 ab
    db  033h, 0dbh
    ; xor bx, bx                                ; 33 db
    add bx, 01000h                            ; 81 c3 00 10
    cmp bx, 09000h                            ; 81 fb 00 90
    jnc short 0e0cdh                          ; 73 0b
    mov es, bx                                ; 8e c3
    db  033h, 0ffh
    ; xor di, di                                ; 33 ff
    mov cx, 08000h                            ; b9 00 80
    rep stosw                                 ; f3 ab
    jmp short 0e0b8h                          ; eb eb
    mov es, bx                                ; 8e c3
    db  033h, 0ffh
    ; xor di, di                                ; 33 ff
    mov cx, 07ff8h                            ; b9 f8 7f
    rep stosw                                 ; f3 ab
    db  033h, 0dbh
    ; xor bx, bx                                ; 33 db
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    call 01707h                               ; e8 29 36
    call 0e8e0h                               ; e8 ff 07
    db  033h, 0dbh
    ; xor bx, bx                                ; 33 db
    mov ds, bx                                ; 8e db
    mov cx, strict word 00060h                ; b9 60 00
    mov ax, 0ff53h                            ; b8 53 ff
    mov dx, 0f000h                            ; ba 00 f0
    call 0e044h                               ; e8 53 ff
    mov bx, 001a0h                            ; bb a0 01
    mov cx, strict word 00010h                ; b9 10 00
    call 0e044h                               ; e8 4a ff
    mov ax, 0027fh                            ; b8 7f 02
    mov word [00413h], ax                     ; a3 13 04
    mov ax, 0f84dh                            ; b8 4d f8
    mov word [00044h], ax                     ; a3 44 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [00046h], ax                     ; a3 46 00
    mov ax, 0f841h                            ; b8 41 f8
    mov word [00048h], ax                     ; a3 48 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [0004ah], ax                     ; a3 4a 00
    mov ax, 0f859h                            ; b8 59 f8
    mov word [00054h], ax                     ; a3 54 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [00056h], ax                     ; a3 56 00
    mov ax, 0efd4h                            ; b8 d4 ef
    mov word [0005ch], ax                     ; a3 5c 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [0005eh], ax                     ; a3 5e 00
    mov ax, 0f0a4h                            ; b8 a4 f0
    mov word [00060h], ax                     ; a3 60 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [00062h], ax                     ; a3 62 00
    mov ax, 0e6f2h                            ; b8 f2 e6
    mov word [00064h], ax                     ; a3 64 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [00066h], ax                     ; a3 66 00
    mov ax, 0efedh                            ; b8 ed ef
    mov word [00070h], ax                     ; a3 70 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [00072h], ax                     ; a3 72 00
    call 0e7c0h                               ; e8 69 06
    mov ax, 0fea5h                            ; b8 a5 fe
    mov word [00020h], ax                     ; a3 20 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [00022h], ax                     ; a3 22 00
    mov AL, strict byte 034h                  ; b0 34
    out strict byte 043h, AL                  ; e6 43
    mov AL, strict byte 000h                  ; b0 00
    out strict byte 040h, AL                  ; e6 40
    out strict byte 040h, AL                  ; e6 40
    mov ax, 0e987h                            ; b8 87 e9
    mov word [00024h], ax                     ; a3 24 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [00026h], ax                     ; a3 26 00
    mov ax, 0e82eh                            ; b8 2e e8
    mov word [00058h], ax                     ; a3 58 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [0005ah], ax                     ; a3 5a 00
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    mov ds, ax                                ; 8e d8
    mov byte [00417h], AL                     ; a2 17 04
    mov byte [00418h], AL                     ; a2 18 04
    mov byte [00419h], AL                     ; a2 19 04
    mov byte [00471h], AL                     ; a2 71 04
    mov byte [00497h], AL                     ; a2 97 04
    mov AL, strict byte 010h                  ; b0 10
    mov byte [00496h], AL                     ; a2 96 04
    mov bx, strict word 0001eh                ; bb 1e 00
    mov word [0041ah], bx                     ; 89 1e 1a 04
    mov word [0041ch], bx                     ; 89 1e 1c 04
    mov word [00480h], bx                     ; 89 1e 80 04
    mov bx, strict word 0003eh                ; bb 3e 00
    mov word [00482h], bx                     ; 89 1e 82 04
    push DS                                   ; 1e
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    call 048d0h                               ; e8 16 67
    pop DS                                    ; 1f
    mov AL, strict byte 014h                  ; b0 14
    out strict byte 070h, AL                  ; e6 70
    in AL, strict byte 071h                   ; e4 71
    mov byte [00410h], AL                     ; a2 10 04
    mov ax, 0ff53h                            ; b8 53 ff
    mov word [0003ch], ax                     ; a3 3c 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [0003eh], ax                     ; a3 3e 00
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    mov ds, ax                                ; 8e d8
    db  033h, 0dbh
    ; xor bx, bx                                ; 33 db
    mov CL, strict byte 014h                  ; b1 14
    mov dx, 00378h                            ; ba 78 03
    call 0ecedh                               ; e8 0f 0b
    mov dx, 00278h                            ; ba 78 02
    call 0ecedh                               ; e8 09 0b
    sal bx, 00eh                              ; c1 e3 0e
    mov ax, word [00410h]                     ; a1 10 04
    and ax, 03fffh                            ; 25 ff 3f
    db  00bh, 0c3h
    ; or ax, bx                                 ; 0b c3
    mov word [00410h], ax                     ; a3 10 04
    mov ax, 0e746h                            ; b8 46 e7
    mov word [0002ch], ax                     ; a3 2c 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [0002eh], ax                     ; a3 2e 00
    mov ax, 0e746h                            ; b8 46 e7
    mov word [00030h], ax                     ; a3 30 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [00032h], ax                     ; a3 32 00
    mov ax, 0e739h                            ; b8 39 e7
    mov word [00050h], ax                     ; a3 50 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [00052h], ax                     ; a3 52 00
    db  033h, 0dbh
    ; xor bx, bx                                ; 33 db
    mov CL, strict byte 00ah                  ; b1 0a
    mov dx, 003f8h                            ; ba f8 03
    call 0ed0bh                               ; e8 eb 0a
    mov dx, 002f8h                            ; ba f8 02
    call 0ed0bh                               ; e8 e5 0a
    mov dx, 003e8h                            ; ba e8 03
    call 0ed0bh                               ; e8 df 0a
    mov dx, 002e8h                            ; ba e8 02
    call 0ed0bh                               ; e8 d9 0a
    sal bx, 009h                              ; c1 e3 09
    mov ax, word [00410h]                     ; a1 10 04
    and ax, 0f1ffh                            ; 25 ff f1
    db  00bh, 0c3h
    ; or ax, bx                                 ; 0b c3
    mov word [00410h], ax                     ; a3 10 04
    mov ax, 0fe6eh                            ; b8 6e fe
    mov word [00068h], ax                     ; a3 68 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [0006ah], ax                     ; a3 6a 00
    mov ax, 0ff53h                            ; b8 53 ff
    mov word [00128h], ax                     ; a3 28 01
    mov ax, 0f000h                            ; b8 00 f0
    mov word [0012ah], ax                     ; a3 2a 01
    mov ax, 0fe8fh                            ; b8 8f fe
    mov word [001c0h], ax                     ; a3 c0 01
    mov ax, 0f000h                            ; b8 00 f0
    mov word [001c2h], ax                     ; a3 c2 01
    call 0edbfh                               ; e8 58 0b
    mov ax, 0f8a4h                            ; b8 a4 f8
    mov word [001d0h], ax                     ; a3 d0 01
    mov ax, 0f000h                            ; b8 00 f0
    mov word [001d2h], ax                     ; a3 d2 01
    mov ax, 0e2cah                            ; b8 ca e2
    mov word [001d4h], ax                     ; a3 d4 01
    mov ax, 0f000h                            ; b8 00 f0
    mov word [001d6h], ax                     ; a3 d6 01
    mov ax, 0f065h                            ; b8 65 f0
    mov word [00040h], ax                     ; a3 40 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [00042h], ax                     ; a3 42 00
    call 0e79bh                               ; e8 0d 05
    call 0f13ch                               ; e8 ab 0e
    call 0f1c1h                               ; e8 2d 0f
    call 0e758h                               ; e8 c1 04
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    call 01b25h                               ; e8 88 38
    call 01f13h                               ; e8 73 3c
    call 08946h                               ; e8 a3 a6
    call 07a5bh                               ; e8 b5 97
    call 0ed2fh                               ; e8 86 0a
    call 0e2d2h                               ; e8 26 00
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    call 01725h                               ; e8 73 34
    call 035e0h                               ; e8 2b 53
    sti                                       ; fb
    int 019h                                  ; cd 19
    sti                                       ; fb
    hlt                                       ; f4
    jmp short 0e2b9h                          ; eb fd
    cli                                       ; fa
    hlt                                       ; f4
    add byte [bx+si], al                      ; 00 00
    add byte [bx+si+04dh], bl                 ; 00 58 4d
nmi:                                         ; 0xfe2c3 LB 0x7
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    call 016e7h                               ; e8 1e 34
    iret                                      ; cf
int75_handler:                               ; 0xfe2ca LB 0x8
    out strict byte 0f0h, AL                  ; e6 f0
    call 0e03bh                               ; e8 6c fd
    int 002h                                  ; cd 02
    iret                                      ; cf
hard_drive_post:                             ; 0xfe2d2 LB 0x12c
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    mov ds, ax                                ; 8e d8
    mov byte [00474h], AL                     ; a2 74 04
    mov byte [00477h], AL                     ; a2 77 04
    mov byte [0048ch], AL                     ; a2 8c 04
    mov byte [0048dh], AL                     ; a2 8d 04
    mov byte [0048eh], AL                     ; a2 8e 04
    mov AL, strict byte 0c0h                  ; b0 c0
    mov byte [00476h], AL                     ; a2 76 04
    mov ax, 0e3feh                            ; b8 fe e3
    mov word [0004ch], ax                     ; a3 4c 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [0004eh], ax                     ; a3 4e 00
    mov ax, 0f8d2h                            ; b8 d2 f8
    mov word [001d8h], ax                     ; a3 d8 01
    mov ax, 0f000h                            ; b8 00 f0
    mov word [001dah], ax                     ; a3 da 01
    mov ax, strict word 0003dh                ; b8 3d 00
    mov word [00104h], ax                     ; a3 04 01
    mov ax, 09fc0h                            ; b8 c0 9f
    mov word [00106h], ax                     ; a3 06 01
    mov ax, strict word 0004dh                ; b8 4d 00
    mov word [00118h], ax                     ; a3 18 01
    mov ax, 09fc0h                            ; b8 c0 9f
    mov word [0011ah], ax                     ; a3 1a 01
    retn                                      ; c3
    times 0xe1 db 0
    db  'XM'
int13_handler:                               ; 0xfe3fe LB 0x3
    jmp near 0ec5bh                           ; e9 5a 08
rom_fdpt:                                    ; 0xfe401 LB 0x2f1
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 058h
    db  04dh
int19_handler:                               ; 0xfe6f2 LB 0x61
    jmp near 0f0ach                           ; e9 b7 09
    or word [bx+si], ax                       ; 09 00
    cld                                       ; fc
    add byte [bx+di], al                      ; 00 01
    je short 0e73ch                           ; 74 40
    times 0x2b db 0
    pop ax                                    ; 58
    dec bp                                    ; 4d
    times 0xe db 0
    pop ax                                    ; 58
    dec bp                                    ; 4d
    push DS                                   ; 1e
    push ES                                   ; 06
    pushaw                                    ; 60
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    call 05c46h                               ; e8 04 75
    popaw                                     ; 61
    pop ES                                    ; 07
    pop DS                                    ; 1f
    iret                                      ; cf
    push DS                                   ; 1e
    push ES                                   ; 06
    pushaw                                    ; 60
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    call 0167eh                               ; e8 2f 2f
    popaw                                     ; 61
    pop ES                                    ; 07
    pop DS                                    ; 1f
    iret                                      ; cf
rom_checksum:                                ; 0xfe753 LB 0x5
    push ax                                   ; 50
    db  032h, 0c0h
    ; xor al, al                                ; 32 c0
    pop ax                                    ; 58
    retn                                      ; c3
rom_scan:                                    ; 0xfe758 LB 0x43
    mov cx, 0c000h                            ; b9 00 c0
    mov ds, cx                                ; 8e d9
    mov ax, strict word 00004h                ; b8 04 00
    cmp word [word 00000h], 0aa55h            ; 81 3e 00 00 55 aa
    jne short 0e78bh                          ; 75 23
    call 0e753h                               ; e8 e8 ff
    jne short 0e78bh                          ; 75 1e
    mov AL, byte [00002h]                     ; a0 02 00
    test AL, strict byte 003h                 ; a8 03
    je short 0e778h                           ; 74 04
    and AL, strict byte 0fch                  ; 24 fc
    add AL, strict byte 004h                  ; 04 04
    db  033h, 0dbh
    ; xor bx, bx                                ; 33 db
    mov ds, bx                                ; 8e db
    push ax                                   ; 50
    push cx                                   ; 51
    push strict byte 00003h                   ; 6a 03
    db  08bh, 0ech
    ; mov bp, sp                                ; 8b ec
    call far [byte bp+000h]                   ; ff 5e 00
    cli                                       ; fa
    add sp, strict byte 00002h                ; 83 c4 02
    pop cx                                    ; 59
    pop ax                                    ; 58
    sal ax, 005h                              ; c1 e0 05
    db  003h, 0c8h
    ; add cx, ax                                ; 03 c8
    cmp cx, 0e800h                            ; 81 f9 00 e8
    jbe short 0e75bh                          ; 76 c5
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    mov ds, ax                                ; 8e d8
    retn                                      ; c3
init_pic:                                    ; 0xfe79b LB 0x25
    mov AL, strict byte 011h                  ; b0 11
    out strict byte 020h, AL                  ; e6 20
    out strict byte 0a0h, AL                  ; e6 a0
    mov AL, strict byte 008h                  ; b0 08
    out strict byte 021h, AL                  ; e6 21
    mov AL, strict byte 070h                  ; b0 70
    out strict byte 0a1h, AL                  ; e6 a1
    mov AL, strict byte 004h                  ; b0 04
    out strict byte 021h, AL                  ; e6 21
    mov AL, strict byte 002h                  ; b0 02
    out strict byte 0a1h, AL                  ; e6 a1
    mov AL, strict byte 001h                  ; b0 01
    out strict byte 021h, AL                  ; e6 21
    out strict byte 0a1h, AL                  ; e6 a1
    mov AL, strict byte 0b8h                  ; b0 b8
    out strict byte 021h, AL                  ; e6 21
    mov AL, strict byte 08fh                  ; b0 8f
    out strict byte 0a1h, AL                  ; e6 a1
    retn                                      ; c3
ebda_post:                                   ; 0xfe7c0 LB 0xa4
    mov ax, 0e746h                            ; b8 46 e7
    mov word [00034h], ax                     ; a3 34 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [00036h], ax                     ; a3 36 00
    mov ax, 0e746h                            ; b8 46 e7
    mov word [0003ch], ax                     ; a3 3c 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [0003eh], ax                     ; a3 3e 00
    mov ax, 0e746h                            ; b8 46 e7
    mov word [001c8h], ax                     ; a3 c8 01
    mov ax, 0f000h                            ; b8 00 f0
    mov word [001cah], ax                     ; a3 ca 01
    mov ax, 0e746h                            ; b8 46 e7
    mov word [001dch], ax                     ; a3 dc 01
    mov ax, 0f000h                            ; b8 00 f0
    mov word [001deh], ax                     ; a3 de 01
    mov ax, 09fc0h                            ; b8 c0 9f
    mov ds, ax                                ; 8e d8
    mov byte [word 00000h], 001h              ; c6 06 00 00 01
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    mov ds, ax                                ; 8e d8
    mov word [0040eh], 09fc0h                 ; c7 06 0e 04 c0 9f
    retn                                      ; c3
    times 0x27 db 0
    pop ax                                    ; 58
    dec bp                                    ; 4d
    sti                                       ; fb
    push ES                                   ; 06
    push DS                                   ; 1e
    pushaw                                    ; 60
    cmp ah, 000h                              ; 80 fc 00
    je short 0e846h                           ; 74 0f
    cmp ah, 010h                              ; 80 fc 10
    je short 0e846h                           ; 74 0a
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    call 04fc2h                               ; e8 80 67
    popaw                                     ; 61
    pop DS                                    ; 1f
    pop ES                                    ; 07
    iret                                      ; cf
    mov bx, strict word 00040h                ; bb 40 00
    mov ds, bx                                ; 8e db
    cli                                       ; fa
    mov bx, word [word 0001ah]                ; 8b 1e 1a 00
    cmp bx, word [word 0001ch]                ; 3b 1e 1c 00
    jne short 0e85ah                          ; 75 04
    sti                                       ; fb
    nop                                       ; 90
    jmp short 0e84bh                          ; eb f1
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    call 04fc2h                               ; e8 62 67
    popaw                                     ; 61
    pop DS                                    ; 1f
    pop ES                                    ; 07
    iret                                      ; cf
pmode_enter:                                 ; 0xfe864 LB 0x1b
    push CS                                   ; 0e
    pop DS                                    ; 1f
    lgdt [cs:0e892h]                          ; 2e 0f 01 16 92 e8
    mov eax, cr0                              ; 0f 20 c0
    or AL, strict byte 001h                   ; 0c 01
    mov cr0, eax                              ; 0f 22 c0
    jmp far 00020h:0e879h                     ; ea 79 e8 20 00
    mov ax, strict word 00018h                ; b8 18 00
    mov ds, ax                                ; 8e d8
    retn                                      ; c3
pmode_exit:                                  ; 0xfe87f LB 0x13
    mov ax, strict word 00040h                ; b8 40 00
    mov ds, ax                                ; 8e d8
    mov eax, cr0                              ; 0f 20 c0
    and AL, strict byte 0feh                  ; 24 fe
    mov cr0, eax                              ; 0f 22 c0
    jmp far 0f000h:0e891h                     ; ea 91 e8 00 f0
    retn                                      ; c3
pmbios_gdt_desc:                             ; 0xfe892 LB 0x6
    db  047h, 000h, 098h, 0e8h, 00fh, 000h
pmbios_gdt:                                  ; 0xfe898 LB 0x48
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  0ffh, 0ffh, 000h, 000h, 000h, 09bh, 0cfh, 000h, 0ffh, 0ffh, 000h, 000h, 000h, 093h, 0cfh, 000h
    db  0ffh, 0ffh, 000h, 000h, 00fh, 09bh, 000h, 000h, 0ffh, 0ffh, 000h, 000h, 000h, 093h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  0ffh, 0ffh, 000h, 004h, 000h, 093h, 000h, 000h
pmode_setup:                                 ; 0xfe8e0 LB 0x37b
    push eax                                  ; 66 50
    push esi                                  ; 66 56
    pushfw                                    ; 9c
    cli                                       ; fa
    call 0e864h                               ; e8 7b ff
    mov eax, cr0                              ; 0f 20 c0
    and eax, strict dword 09fffffffh          ; 66 25 ff ff ff 9f
    mov cr0, eax                              ; 0f 22 c0
    mov esi, strict dword 0fee00350h          ; 66 be 50 03 e0 fe
    mov eax, dword [esi]                      ; 67 66 8b 06
    and eax, strict dword 0fffe00ffh          ; 66 25 ff 00 fe ff
    or ah, 007h                               ; 80 cc 07
    mov dword [esi], eax                      ; 67 66 89 06
    mov esi, strict dword 0fee00360h          ; 66 be 60 03 e0 fe
    mov eax, dword [esi]                      ; 67 66 8b 06
    and eax, strict dword 0fffe00ffh          ; 66 25 ff 00 fe ff
    or ah, 004h                               ; 80 cc 04
    mov dword [esi], eax                      ; 67 66 89 06
    call 0e87fh                               ; e8 59 ff
    popfw                                     ; 9d
    pop esi                                   ; 66 5e
    pop eax                                   ; 66 58
    retn                                      ; c3
    times 0x59 db 0
    pop ax                                    ; 58
    dec bp                                    ; 4d
    cli                                       ; fa
    push ax                                   ; 50
    mov AL, strict byte 0adh                  ; b0 ad
    out strict byte 064h, AL                  ; e6 64
    mov AL, strict byte 00bh                  ; b0 0b
    out strict byte 020h, AL                  ; e6 20
    in AL, strict byte 020h                   ; e4 20
    and AL, strict byte 002h                  ; 24 02
    je short 0e9d6h                           ; 74 3f
    in AL, strict byte 060h                   ; e4 60
    push DS                                   ; 1e
    pushaw                                    ; 60
    cld                                       ; fc
    mov AH, strict byte 04fh                  ; b4 4f
    stc                                       ; f9
    int 015h                                  ; cd 15
    jnc short 0e9d0h                          ; 73 2d
    sti                                       ; fb
    cmp AL, strict byte 0e0h                  ; 3c e0
    jne short 0e9b6h                          ; 75 0e
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    mov ds, ax                                ; 8e d8
    mov AL, byte [00496h]                     ; a0 96 04
    or AL, strict byte 002h                   ; 0c 02
    mov byte [00496h], AL                     ; a2 96 04
    jmp short 0e9d0h                          ; eb 1a
    cmp AL, strict byte 0e1h                  ; 3c e1
    jne short 0e9c8h                          ; 75 0e
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    mov ds, ax                                ; 8e d8
    mov AL, byte [00496h]                     ; a0 96 04
    or AL, strict byte 001h                   ; 0c 01
    mov byte [00496h], AL                     ; a2 96 04
    jmp short 0e9d0h                          ; eb 08
    push ES                                   ; 06
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    call 04bdah                               ; e8 0b 62
    pop ES                                    ; 07
    popaw                                     ; 61
    pop DS                                    ; 1f
    cli                                       ; fa
    call 0e03fh                               ; e8 69 f6
    mov AL, strict byte 0aeh                  ; b0 ae
    out strict byte 064h, AL                  ; e6 64
    pop ax                                    ; 58
    iret                                      ; cf
    times 0x27b db 0
    pop ax                                    ; 58
    dec bp                                    ; 4d
    jmp short 0ecb0h                          ; eb 55
int13_relocated:                             ; 0xfec5b LB 0x55
    cmp ah, 04ah                              ; 80 fc 4a
    jc short 0ec71h                           ; 72 11
    cmp ah, 04dh                              ; 80 fc 4d
    jnbe short 0ec71h                         ; 77 0c
    pushaw                                    ; 60
    push ES                                   ; 06
    push DS                                   ; 1e
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    push 0ece9h                               ; 68 e9 ec
    jmp near 0361eh                           ; e9 ad 49
    push ES                                   ; 06
    push ax                                   ; 50
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    call 035f6h                               ; e8 7d 49
    cmp AL, strict byte 000h                  ; 3c 00
    je short 0ecabh                           ; 74 2e
    call 0360ah                               ; e8 8a 49
    pop dx                                    ; 5a
    push dx                                   ; 52
    db  03ah, 0c2h
    ; cmp al, dl                                ; 3a c2
    jne short 0ec97h                          ; 75 11
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    pop ax                                    ; 58
    pop ES                                    ; 07
    pushaw                                    ; 60
    push ES                                   ; 06
    push DS                                   ; 1e
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    push 0ece9h                               ; 68 e9 ec
    jmp near 03be6h                           ; e9 4f 4f
    and dl, 0e0h                              ; 80 e2 e0
    db  03ah, 0c2h
    ; cmp al, dl                                ; 3a c2
    jne short 0ecabh                          ; 75 0d
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    pop ax                                    ; 58
    pop ES                                    ; 07
    push ax                                   ; 50
    push cx                                   ; 51
    push dx                                   ; 52
    push bx                                   ; 53
    db  0feh, 0cah
    ; dec dl                                    ; fe ca
    jmp short 0ecb4h                          ; eb 09
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    pop ax                                    ; 58
    pop ES                                    ; 07
int13_noeltorito:                            ; 0xfecb0 LB 0x4
    push ax                                   ; 50
    push cx                                   ; 51
    push dx                                   ; 52
    push bx                                   ; 53
int13_legacy:                                ; 0xfecb4 LB 0x14
    push dx                                   ; 52
    push bp                                   ; 55
    push si                                   ; 56
    push di                                   ; 57
    push ES                                   ; 06
    push DS                                   ; 1e
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    test dl, 080h                             ; f6 c2 80
    jne short 0ecc8h                          ; 75 06
    push 0ece9h                               ; 68 e9 ec
    jmp near 02e51h                           ; e9 89 41
int13_notfloppy:                             ; 0xfecc8 LB 0x14
    cmp dl, 0e0h                              ; 80 fa e0
    jc short 0ecdch                           ; 72 0f
    shr ebx, 010h                             ; 66 c1 eb 10
    push bx                                   ; 53
    call 04015h                               ; e8 40 53
    pop bx                                    ; 5b
    sal ebx, 010h                             ; 66 c1 e3 10
    jmp short 0ece9h                          ; eb 0d
int13_disk:                                  ; 0xfecdc LB 0xd
    cmp ah, 040h                              ; 80 fc 40
    jnbe short 0ece6h                         ; 77 05
    call 05342h                               ; e8 5e 66
    jmp short 0ece9h                          ; eb 03
    call 0577eh                               ; e8 95 6a
int13_out:                                   ; 0xfece9 LB 0x4
    pop DS                                    ; 1f
    pop ES                                    ; 07
    popaw                                     ; 61
    iret                                      ; cf
detect_parport:                              ; 0xfeced LB 0x1e
    push dx                                   ; 52
    inc dx                                    ; 42
    inc dx                                    ; 42
    in AL, DX                                 ; ec
    and AL, strict byte 0dfh                  ; 24 df
    out DX, AL                                ; ee
    pop dx                                    ; 5a
    mov AL, strict byte 0aah                  ; b0 aa
    out DX, AL                                ; ee
    in AL, DX                                 ; ec
    cmp AL, strict byte 0aah                  ; 3c aa
    jne short 0ed0ah                          ; 75 0d
    push bx                                   ; 53
    sal bx, 1                                 ; d1 e3
    mov word [bx+00408h], dx                  ; 89 97 08 04
    pop bx                                    ; 5b
    mov byte [bx+00478h], cl                  ; 88 8f 78 04
    inc bx                                    ; 43
    retn                                      ; c3
detect_serial:                               ; 0xfed0b LB 0x24
    push dx                                   ; 52
    inc dx                                    ; 42
    mov AL, strict byte 002h                  ; b0 02
    out DX, AL                                ; ee
    in AL, DX                                 ; ec
    cmp AL, strict byte 002h                  ; 3c 02
    jne short 0ed2dh                          ; 75 18
    inc dx                                    ; 42
    in AL, DX                                 ; ec
    cmp AL, strict byte 002h                  ; 3c 02
    jne short 0ed2dh                          ; 75 12
    dec dx                                    ; 4a
    db  032h, 0c0h
    ; xor al, al                                ; 32 c0
    pop dx                                    ; 5a
    push bx                                   ; 53
    sal bx, 1                                 ; d1 e3
    mov word [bx+00400h], dx                  ; 89 97 00 04
    pop bx                                    ; 5b
    mov byte [bx+0047ch], cl                  ; 88 8f 7c 04
    inc bx                                    ; 43
    retn                                      ; c3
    pop dx                                    ; 5a
    retn                                      ; c3
floppy_post:                                 ; 0xfed2f LB 0x87
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    mov ds, ax                                ; 8e d8
    mov AL, strict byte 000h                  ; b0 00
    mov byte [0043eh], AL                     ; a2 3e 04
    mov byte [0043fh], AL                     ; a2 3f 04
    mov byte [00440h], AL                     ; a2 40 04
    mov byte [00441h], AL                     ; a2 41 04
    mov byte [00442h], AL                     ; a2 42 04
    mov byte [00443h], AL                     ; a2 43 04
    mov byte [00444h], AL                     ; a2 44 04
    mov byte [00445h], AL                     ; a2 45 04
    mov byte [00446h], AL                     ; a2 46 04
    mov byte [00447h], AL                     ; a2 47 04
    mov byte [00448h], AL                     ; a2 48 04
    mov byte [0048bh], AL                     ; a2 8b 04
    mov AL, strict byte 010h                  ; b0 10
    out strict byte 070h, AL                  ; e6 70
    in AL, strict byte 071h                   ; e4 71
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0
    shr al, 004h                              ; c0 e8 04
    je short 0ed6ah                           ; 74 04
    mov BL, strict byte 007h                  ; b3 07
    jmp short 0ed6ch                          ; eb 02
    mov BL, strict byte 000h                  ; b3 00
    db  08ah, 0c4h
    ; mov al, ah                                ; 8a c4
    and AL, strict byte 00fh                  ; 24 0f
    je short 0ed75h                           ; 74 03
    or bl, 070h                               ; 80 cb 70
    mov byte [0048fh], bl                     ; 88 1e 8f 04
    mov AL, strict byte 000h                  ; b0 00
    mov byte [00490h], AL                     ; a2 90 04
    mov byte [00491h], AL                     ; a2 91 04
    mov byte [00492h], AL                     ; a2 92 04
    mov byte [00493h], AL                     ; a2 93 04
    mov byte [00494h], AL                     ; a2 94 04
    mov byte [00495h], AL                     ; a2 95 04
    mov AL, strict byte 002h                  ; b0 02
    out strict byte 00ah, AL                  ; e6 0a
    mov ax, 0efc7h                            ; b8 c7 ef
    mov word [00078h], ax                     ; a3 78 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [0007ah], ax                     ; a3 7a 00
    mov ax, 0ec59h                            ; b8 59 ec
    mov word [00100h], ax                     ; a3 00 01
    mov ax, 0f000h                            ; b8 00 f0
    mov word [00102h], ax                     ; a3 02 01
    mov ax, 0ef57h                            ; b8 57 ef
    mov word [00038h], ax                     ; a3 38 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [0003ah], ax                     ; a3 3a 00
    retn                                      ; c3
bcd_to_bin:                                  ; 0xfedb6 LB 0x9
    sal ax, 004h                              ; c1 e0 04
    shr al, 004h                              ; c0 e8 04
    aad 00ah                                  ; d5 0a
    retn                                      ; c3
rtc_post:                                    ; 0xfedbf LB 0x198
    db  066h, 033h, 0c0h
    ; xor eax, eax                              ; 66 33 c0
    mov AL, strict byte 000h                  ; b0 00
    out strict byte 070h, AL                  ; e6 70
    in AL, strict byte 071h                   ; e4 71
    call 0edb6h                               ; e8 eb ff
    mov edx, strict dword 00115cf2bh          ; 66 ba 2b cf 15 01
    mul edx                                   ; 66 f7 e2
    mov ebx, strict dword 0000f4240h          ; 66 bb 40 42 0f 00
    db  066h, 033h, 0d2h
    ; xor edx, edx                              ; 66 33 d2
    div ebx                                   ; 66 f7 f3
    db  066h, 08bh, 0c8h
    ; mov ecx, eax                              ; 66 8b c8
    db  066h, 033h, 0c0h
    ; xor eax, eax                              ; 66 33 c0
    mov AL, strict byte 002h                  ; b0 02
    out strict byte 070h, AL                  ; e6 70
    in AL, strict byte 071h                   ; e4 71
    call 0edb6h                               ; e8 c7 ff
    mov edx, strict dword 000a6af80h          ; 66 ba 80 af a6 00
    mul edx                                   ; 66 f7 e2
    mov ebx, strict dword 000002710h          ; 66 bb 10 27 00 00
    db  066h, 033h, 0d2h
    ; xor edx, edx                              ; 66 33 d2
    div ebx                                   ; 66 f7 f3
    db  066h, 003h, 0c8h
    ; add ecx, eax                              ; 66 03 c8
    db  066h, 033h, 0c0h
    ; xor eax, eax                              ; 66 33 c0
    mov AL, strict byte 004h                  ; b0 04
    out strict byte 070h, AL                  ; e6 70
    in AL, strict byte 071h                   ; e4 71
    call 0edb6h                               ; e8 a3 ff
    mov edx, strict dword 003e81d03h          ; 66 ba 03 1d e8 03
    mul edx                                   ; 66 f7 e2
    mov ebx, strict dword 0000003e8h          ; 66 bb e8 03 00 00
    db  066h, 033h, 0d2h
    ; xor edx, edx                              ; 66 33 d2
    div ebx                                   ; 66 f7 f3
    db  066h, 003h, 0c8h
    ; add ecx, eax                              ; 66 03 c8
    mov dword [0046ch], ecx                   ; 66 89 0e 6c 04
    db  032h, 0c0h
    ; xor al, al                                ; 32 c0
    mov byte [00470h], AL                     ; a2 70 04
    retn                                      ; c3
    times 0x11f db 0
    db  'XM'
int0e_handler:                               ; 0xfef57 LB 0x70
    push ax                                   ; 50
    push dx                                   ; 52
    mov dx, 003f4h                            ; ba f4 03
    in AL, DX                                 ; ec
    and AL, strict byte 0c0h                  ; 24 c0
    cmp AL, strict byte 0c0h                  ; 3c c0
    je short 0ef81h                           ; 74 1e
    mov dx, 003f5h                            ; ba f5 03
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, 003f4h                            ; ba f4 03
    in AL, DX                                 ; ec
    and AL, strict byte 0c0h                  ; 24 c0
    cmp AL, strict byte 0c0h                  ; 3c c0
    jne short 0ef69h                          ; 75 f6
    mov dx, 003f5h                            ; ba f5 03
    in AL, DX                                 ; ec
    mov dx, 003f4h                            ; ba f4 03
    in AL, DX                                 ; ec
    and AL, strict byte 0c0h                  ; 24 c0
    cmp AL, strict byte 0c0h                  ; 3c c0
    je short 0ef73h                           ; 74 f2
    push DS                                   ; 1e
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    mov ds, ax                                ; 8e d8
    call 0e03fh                               ; e8 b6 f0
    or byte [0043eh], 080h                    ; 80 0e 3e 04 80
    pop DS                                    ; 1f
    pop dx                                    ; 5a
    pop ax                                    ; 58
    iret                                      ; cf
    times 0x33 db 0
    db  'XM'
_diskette_param_table:                       ; 0xfefc7 LB 0xd
    scasw                                     ; af
    add ah, byte [di]                         ; 02 25
    add dl, byte [bp+si]                      ; 02 12
    db  01bh, 0ffh
    ; sbb di, di                                ; 1b ff
    insb                                      ; 6c
    db  0f6h
    invd                                      ; 0f 08
    jmp short 0efd4h                          ; eb 00
int17_handler:                               ; 0xfefd4 LB 0xd
    push DS                                   ; 1e
    push ES                                   ; 06
    pushaw                                    ; 60
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    call 06dc4h                               ; e8 e7 7d
    popaw                                     ; 61
    pop ES                                    ; 07
    pop DS                                    ; 1f
    iret                                      ; cf
_pmode_IDT:                                  ; 0xfefe1 LB 0x6
    db  000h, 000h, 000h, 000h, 00fh, 000h
_rmode_IDT:                                  ; 0xfefe7 LB 0x6
    db  0ffh, 003h, 000h, 000h, 000h, 000h
int1c_handler:                               ; 0xfefed LB 0x78
    iret                                      ; cf
    times 0x55 db 0
    pop ax                                    ; 58
    dec bp                                    ; 4d
    iret                                      ; cf
    times 0x1d db 0
    db  'XM'
int10_handler:                               ; 0xff065 LB 0x47
    iret                                      ; cf
    times 0x3c db 0
    pop ax                                    ; 58
    dec bp                                    ; 4d
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    call 016f7h                               ; e8 4d 26
    hlt                                       ; f4
    iret                                      ; cf
int19_relocated:                             ; 0xff0ac LB 0x90
    db  08bh, 0ech
    ; mov bp, sp                                ; 8b ec
    mov ax, word [bp+002h]                    ; 8b 46 02
    cmp ax, 0f000h                            ; 3d 00 f0
    je short 0f0c3h                           ; 74 0d
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    mov ds, ax                                ; 8e d8
    mov ax, 01234h                            ; b8 34 12
    mov word [001d8h], ax                     ; a3 d8 01
    jmp near 0e05bh                           ; e9 98 ef
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    push bp                                   ; 55
    db  08bh, 0ech
    ; mov bp, sp                                ; 8b ec
    mov ax, strict word 00001h                ; b8 01 00
    push ax                                   ; 50
    call 0466eh                               ; e8 9e 55
    inc sp                                    ; 44
    inc sp                                    ; 44
    test ax, ax                               ; 85 c0
    jne short 0f0feh                          ; 75 28
    mov ax, strict word 00002h                ; b8 02 00
    push ax                                   ; 50
    call 0466eh                               ; e8 91 55
    inc sp                                    ; 44
    inc sp                                    ; 44
    test ax, ax                               ; 85 c0
    jne short 0f0feh                          ; 75 1b
    mov ax, strict word 00003h                ; b8 03 00
    push strict byte 00003h                   ; 6a 03
    call 0466eh                               ; e8 83 55
    inc sp                                    ; 44
    inc sp                                    ; 44
    test ax, ax                               ; 85 c0
    jne short 0f0feh                          ; 75 0d
    mov ax, strict word 00004h                ; b8 04 00
    push ax                                   ; 50
    call 0466eh                               ; e8 76 55
    inc sp                                    ; 44
    inc sp                                    ; 44
    test ax, ax                               ; 85 c0
    je short 0f0a4h                           ; 74 a6
    sal eax, 004h                             ; 66 c1 e0 04
    mov word [bp+002h], ax                    ; 89 46 02
    shr eax, 004h                             ; 66 c1 e8 04
    and ax, 0f000h                            ; 25 00 f0
    mov word [bp+004h], ax                    ; 89 46 04
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    mov ds, ax                                ; 8e d8
    mov es, ax                                ; 8e c0
    mov word [byte bp+000h], ax               ; 89 46 00
    mov ax, 0aa55h                            ; b8 55 aa
    pop bp                                    ; 5d
    iret                                      ; cf
    or cx, word [bp+si]                       ; 0b 0a
    or word [bp+di], cx                       ; 09 0b
    push eax                                  ; 66 50
    mov eax, strict dword 000800000h          ; 66 b8 00 00 80 00
    db  08bh, 0c3h
    ; mov ax, bx                                ; 8b c3
    sal eax, 008h                             ; 66 c1 e0 08
    and dl, 0fch                              ; 80 e2 fc
    db  00ah, 0c2h
    ; or al, dl                                 ; 0a c2
    mov dx, 00cf8h                            ; ba f8 0c
    out DX, eax                               ; 66 ef
    pop eax                                   ; 66 58
    retn                                      ; c3
pcibios_init_iomem_bases:                    ; 0xff13c LB 0x16
    push bp                                   ; 55
    db  08bh, 0ech
    ; mov bp, sp                                ; 8b ec
    mov eax, strict dword 0e0000000h          ; 66 b8 00 00 00 e0
    push eax                                  ; 66 50
    mov ax, 0d000h                            ; b8 00 d0
    push ax                                   ; 50
    mov ax, strict word 00010h                ; b8 10 00
    push ax                                   ; 50
    mov bx, strict word 00008h                ; bb 08 00
pci_init_io_loop1:                           ; 0xff152 LB 0xe
    mov DL, strict byte 000h                  ; b2 00
    call 0f121h                               ; e8 ca ff
    mov dx, 00cfch                            ; ba fc 0c
    in ax, DX                                 ; ed
    cmp ax, strict byte 0ffffh                ; 83 f8 ff
    je short 0f199h                           ; 74 39
enable_iomem_space:                          ; 0xff160 LB 0x39
    mov DL, strict byte 004h                  ; b2 04
    call 0f121h                               ; e8 bc ff
    mov dx, 00cfch                            ; ba fc 0c
    in AL, DX                                 ; ec
    or AL, strict byte 007h                   ; 0c 07
    out DX, AL                                ; ee
    mov DL, strict byte 000h                  ; b2 00
    call 0f121h                               ; e8 b0 ff
    mov dx, 00cfch                            ; ba fc 0c
    in eax, DX                                ; 66 ed
    cmp eax, strict dword 020001022h          ; 66 3d 22 10 00 20
    jne short 0f199h                          ; 75 1b
    mov DL, strict byte 010h                  ; b2 10
    call 0f121h                               ; e8 9e ff
    mov dx, 00cfch                            ; ba fc 0c
    in ax, DX                                 ; ed
    and ax, strict byte 0fffch                ; 83 e0 fc
    db  08bh, 0c8h
    ; mov cx, ax                                ; 8b c8
    db  08bh, 0d1h
    ; mov dx, cx                                ; 8b d1
    add dx, strict byte 00014h                ; 83 c2 14
    in ax, DX                                 ; ed
    db  08bh, 0d1h
    ; mov dx, cx                                ; 8b d1
    add dx, strict byte 00018h                ; 83 c2 18
    in eax, DX                                ; 66 ed
next_pci_dev:                                ; 0xff199 LB 0xf
    mov byte [bp-008h], 010h                  ; c6 46 f8 10
    inc bx                                    ; 43
    cmp bx, 00100h                            ; 81 fb 00 01
    jne short 0f152h                          ; 75 ae
    db  08bh, 0e5h
    ; mov sp, bp                                ; 8b e5
    pop bp                                    ; 5d
    retn                                      ; c3
pcibios_init_set_elcr:                       ; 0xff1a8 LB 0xc
    push ax                                   ; 50
    push cx                                   ; 51
    mov dx, 004d0h                            ; ba d0 04
    test AL, strict byte 008h                 ; a8 08
    je short 0f1b4h                           ; 74 03
    inc dx                                    ; 42
    and AL, strict byte 007h                  ; 24 07
is_master_pic:                               ; 0xff1b4 LB 0xd
    db  08ah, 0c8h
    ; mov cl, al                                ; 8a c8
    mov BL, strict byte 001h                  ; b3 01
    sal bl, CL                                ; d2 e3
    in AL, DX                                 ; ec
    db  00ah, 0c3h
    ; or al, bl                                 ; 0a c3
    out DX, AL                                ; ee
    pop cx                                    ; 59
    pop ax                                    ; 58
    retn                                      ; c3
pcibios_init_irqs:                           ; 0xff1c1 LB 0x53
    push DS                                   ; 1e
    push bp                                   ; 55
    mov ax, 0f000h                            ; b8 00 f0
    mov ds, ax                                ; 8e d8
    mov dx, 004d0h                            ; ba d0 04
    mov AL, strict byte 000h                  ; b0 00
    out DX, AL                                ; ee
    inc dx                                    ; 42
    out DX, AL                                ; ee
    mov si, 0f2a0h                            ; be a0 f2
    mov bh, byte [si+008h]                    ; 8a 7c 08
    mov bl, byte [si+009h]                    ; 8a 5c 09
    mov DL, strict byte 000h                  ; b2 00
    call 0f121h                               ; e8 43 ff
    mov dx, 00cfch                            ; ba fc 0c
    in eax, DX                                ; 66 ed
    cmp eax, dword [si+00ch]                  ; 66 3b 44 0c
    jne near 0f291h                           ; 0f 85 a6 00
    mov dl, byte [si+022h]                    ; 8a 54 22
    call 0f121h                               ; e8 30 ff
    push bx                                   ; 53
    mov dx, 00cfch                            ; ba fc 0c
    mov ax, 08080h                            ; b8 80 80
    out DX, ax                                ; ef
    add dx, strict byte 00002h                ; 83 c2 02
    out DX, ax                                ; ef
    mov ax, word [si+006h]                    ; 8b 44 06
    sub ax, strict byte 00020h                ; 83 e8 20
    shr ax, 004h                              ; c1 e8 04
    db  08bh, 0c8h
    ; mov cx, ax                                ; 8b c8
    add si, strict byte 00020h                ; 83 c6 20
    db  08bh, 0ech
    ; mov bp, sp                                ; 8b ec
    mov ax, 0f11dh                            ; b8 1d f1
    push ax                                   ; 50
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    push ax                                   ; 50
pci_init_irq_loop1:                          ; 0xff214 LB 0x5
    mov bh, byte [si]                         ; 8a 3c
    mov bl, byte [si+001h]                    ; 8a 5c 01
pci_init_irq_loop2:                          ; 0xff219 LB 0x15
    mov DL, strict byte 000h                  ; b2 00
    call 0f121h                               ; e8 03 ff
    mov dx, 00cfch                            ; ba fc 0c
    in ax, DX                                 ; ed
    cmp ax, strict byte 0ffffh                ; 83 f8 ff
    jne short 0f22eh                          ; 75 07
    test bl, 007h                             ; f6 c3 07
    je short 0f285h                           ; 74 59
    jmp short 0f27bh                          ; eb 4d
pci_test_int_pin:                            ; 0xff22e LB 0x3c
    mov DL, strict byte 03ch                  ; b2 3c
    call 0f121h                               ; e8 ee fe
    mov dx, 00cfdh                            ; ba fd 0c
    in AL, DX                                 ; ec
    and AL, strict byte 007h                  ; 24 07
    je short 0f27bh                           ; 74 40
    db  0feh, 0c8h
    ; dec al                                    ; fe c8
    mov DL, strict byte 003h                  ; b2 03
    mul dl                                    ; f6 e2
    add AL, strict byte 002h                  ; 04 02
    db  032h, 0e4h
    ; xor ah, ah                                ; 32 e4
    db  08bh, 0d8h
    ; mov bx, ax                                ; 8b d8
    mov al, byte [bx+si]                      ; 8a 00
    db  08ah, 0d0h
    ; mov dl, al                                ; 8a d0
    mov bx, word [byte bp+000h]               ; 8b 5e 00
    call 0f121h                               ; e8 d0 fe
    mov dx, 00cfch                            ; ba fc 0c
    and AL, strict byte 003h                  ; 24 03
    db  002h, 0d0h
    ; add dl, al                                ; 02 d0
    in AL, DX                                 ; ec
    cmp AL, strict byte 080h                  ; 3c 80
    jc short 0f26ah                           ; 72 0d
    mov bx, word [bp-002h]                    ; 8b 5e fe
    mov al, byte [bx]                         ; 8a 07
    out DX, AL                                ; ee
    inc bx                                    ; 43
    mov word [bp-002h], bx                    ; 89 5e fe
    call 0f1a8h                               ; e8 3e ff
pirq_found:                                  ; 0xff26a LB 0x11
    mov bh, byte [si]                         ; 8a 3c
    mov bl, byte [si+001h]                    ; 8a 5c 01
    add bl, byte [bp-003h]                    ; 02 5e fd
    mov DL, strict byte 03ch                  ; b2 3c
    call 0f121h                               ; e8 aa fe
    mov dx, 00cfch                            ; ba fc 0c
    out DX, AL                                ; ee
next_pci_func:                               ; 0xff27b LB 0xa
    inc byte [bp-003h]                        ; fe 46 fd
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3
    test bl, 007h                             ; f6 c3 07
    jne short 0f219h                          ; 75 94
next_pir_entry:                              ; 0xff285 LB 0xc
    add si, strict byte 00010h                ; 83 c6 10
    mov byte [bp-003h], 000h                  ; c6 46 fd 00
    loop 0f214h                               ; e2 86
    db  08bh, 0e5h
    ; mov sp, bp                                ; 8b e5
    pop bx                                    ; 5b
pci_init_end:                                ; 0xff291 LB 0x2f
    pop bp                                    ; 5d
    pop DS                                    ; 1f
    retn                                      ; c3
    db  089h, 0c0h, 089h, 0c0h, 089h, 0c0h, 089h, 0c0h, 089h, 0c0h, 089h, 0c0h, 024h, 050h, 049h, 052h
    db  000h, 001h, 000h, 002h, 000h, 008h, 000h, 000h, 086h, 080h, 000h, 070h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 031h
_pci_routing_table:                          ; 0xff2c0 LB 0x581
    db  000h, 008h, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 000h, 000h
    db  000h, 010h, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 001h, 000h
    db  000h, 018h, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 002h, 000h
    db  000h, 020h, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 003h, 000h
    db  000h, 028h, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 004h, 000h
    db  000h, 030h, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 005h, 000h
    db  000h, 038h, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 006h, 000h
    db  000h, 040h, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 007h, 000h
    db  000h, 048h, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 008h, 000h
    db  000h, 050h, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 009h, 000h
    db  000h, 058h, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 00ah, 000h
    db  000h, 060h, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 00bh, 000h
    db  000h, 068h, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 00ch, 000h
    db  000h, 070h, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 00dh, 000h
    db  000h, 078h, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 00eh, 000h
    db  000h, 080h, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 00fh, 000h
    db  000h, 088h, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 010h, 000h
    db  000h, 090h, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 011h, 000h
    db  000h, 098h, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 012h, 000h
    db  000h, 0a0h, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 013h, 000h
    db  000h, 0a8h, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 014h, 000h
    db  000h, 0b0h, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 015h, 000h
    db  000h, 0b8h, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 016h, 000h
    db  000h, 0c0h, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 017h, 000h
    db  000h, 0c8h, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 018h, 000h
    db  000h, 0d0h, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 019h, 000h
    db  000h, 0d8h, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 01ah, 000h
    db  000h, 0e0h, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 01bh, 000h
    db  000h, 0e8h, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 01ch, 000h
    db  000h, 0f0h, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 01dh, 000h
    db  0e0h, 001h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 058h
    db  04dh
int12_handler:                               ; 0xff841 LB 0xc
    sti                                       ; fb
    push DS                                   ; 1e
    mov ax, strict word 00040h                ; b8 40 00
    mov ds, ax                                ; 8e d8
    mov ax, word [00013h]                     ; a1 13 00
    pop DS                                    ; 1f
    iret                                      ; cf
int11_handler:                               ; 0xff84d LB 0xc
    sti                                       ; fb
    push DS                                   ; 1e
    mov ax, strict word 00040h                ; b8 40 00
    mov ds, ax                                ; 8e d8
    mov ax, word [00010h]                     ; a1 10 00
    pop DS                                    ; 1f
    iret                                      ; cf
int15_handler:                               ; 0xff859 LB 0x29
    pushfw                                    ; 9c
    push DS                                   ; 1e
    push ES                                   ; 06
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    cmp ah, 086h                              ; 80 fc 86
    je short 0f887h                           ; 74 23
    cmp ah, 0e8h                              ; 80 fc e8
    je short 0f887h                           ; 74 1e
    pushaw                                    ; 60
    cmp ah, 053h                              ; 80 fc 53
    je short 0f87dh                           ; 74 0e
    cmp ah, 0c2h                              ; 80 fc c2
    je short 0f882h                           ; 74 0e
    call 05e4fh                               ; e8 d8 65
    popaw                                     ; 61
    pop ES                                    ; 07
    pop DS                                    ; 1f
    popfw                                     ; 9d
    jmp short 0f890h                          ; eb 13
    call 08a75h                               ; e8 f5 91
    jmp short 0f877h                          ; eb f5
int15_handler_mouse:                         ; 0xff882 LB 0x5
    call 06a3eh                               ; e8 b9 71
    jmp short 0f877h                          ; eb f0
int15_handler32:                             ; 0xff887 LB 0x9
    pushad                                    ; 66 60
    call 06318h                               ; e8 8c 6a
    popad                                     ; 66 61
    jmp short 0f878h                          ; eb e8
iret_modify_cf:                              ; 0xff890 LB 0x14
    jc short 0f89bh                           ; 72 09
    push bp                                   ; 55
    db  08bh, 0ech
    ; mov bp, sp                                ; 8b ec
    and byte [bp+006h], 0feh                  ; 80 66 06 fe
    pop bp                                    ; 5d
    iret                                      ; cf
    push bp                                   ; 55
    db  08bh, 0ech
    ; mov bp, sp                                ; 8b ec
    or byte [bp+006h], 001h                   ; 80 4e 06 01
    pop bp                                    ; 5d
    iret                                      ; cf
int74_handler:                               ; 0xff8a4 LB 0x2e
    sti                                       ; fb
    pushaw                                    ; 60
    push ES                                   ; 06
    push DS                                   ; 1e
    push strict byte 00000h                   ; 6a 00
    push strict byte 00000h                   ; 6a 00
    push strict byte 00000h                   ; 6a 00
    push strict byte 00000h                   ; 6a 00
    push strict byte 00000h                   ; 6a 00
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    call 06978h                               ; e8 c0 70
    pop cx                                    ; 59
    jcxz 0f8c7h                               ; e3 0c
    push strict byte 00000h                   ; 6a 00
    pop DS                                    ; 1f
    push word [0040eh]                        ; ff 36 0e 04
    pop DS                                    ; 1f
    call far [word 00022h]                    ; ff 1e 22 00
    cli                                       ; fa
    call 0e03bh                               ; e8 70 e7
    add sp, strict byte 00008h                ; 83 c4 08
    pop DS                                    ; 1f
    pop ES                                    ; 07
    popaw                                     ; 61
    iret                                      ; cf
int76_handler:                               ; 0xff8d2 LB 0x19c
    push ax                                   ; 50
    push DS                                   ; 1e
    mov ax, strict word 00040h                ; b8 40 00
    mov ds, ax                                ; 8e d8
    mov byte [0008eh], 0ffh                   ; c6 06 8e 00 ff
    call 0e03bh                               ; e8 5a e7
    pop DS                                    ; 1f
    pop ax                                    ; 58
    iret                                      ; cf
    times 0x188 db 0
    db  'XM'
font8x8:                                     ; 0xffa6e LB 0x421
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 07eh, 081h, 0a5h, 081h, 0bdh, 099h, 081h, 07eh
    db  07eh, 0ffh, 0dbh, 0ffh, 0c3h, 0e7h, 0ffh, 07eh, 06ch, 0feh, 0feh, 0feh, 07ch, 038h, 010h, 000h
    db  010h, 038h, 07ch, 0feh, 07ch, 038h, 010h, 000h, 038h, 07ch, 038h, 0feh, 0feh, 07ch, 038h, 07ch
    db  010h, 010h, 038h, 07ch, 0feh, 07ch, 038h, 07ch, 000h, 000h, 018h, 03ch, 03ch, 018h, 000h, 000h
    db  0ffh, 0ffh, 0e7h, 0c3h, 0c3h, 0e7h, 0ffh, 0ffh, 000h, 03ch, 066h, 042h, 042h, 066h, 03ch, 000h
    db  0ffh, 0c3h, 099h, 0bdh, 0bdh, 099h, 0c3h, 0ffh, 00fh, 007h, 00fh, 07dh, 0cch, 0cch, 0cch, 078h
    db  03ch, 066h, 066h, 066h, 03ch, 018h, 07eh, 018h, 03fh, 033h, 03fh, 030h, 030h, 070h, 0f0h, 0e0h
    db  07fh, 063h, 07fh, 063h, 063h, 067h, 0e6h, 0c0h, 099h, 05ah, 03ch, 0e7h, 0e7h, 03ch, 05ah, 099h
    db  080h, 0e0h, 0f8h, 0feh, 0f8h, 0e0h, 080h, 000h, 002h, 00eh, 03eh, 0feh, 03eh, 00eh, 002h, 000h
    db  018h, 03ch, 07eh, 018h, 018h, 07eh, 03ch, 018h, 066h, 066h, 066h, 066h, 066h, 000h, 066h, 000h
    db  07fh, 0dbh, 0dbh, 07bh, 01bh, 01bh, 01bh, 000h, 03eh, 063h, 038h, 06ch, 06ch, 038h, 0cch, 078h
    db  000h, 000h, 000h, 000h, 07eh, 07eh, 07eh, 000h, 018h, 03ch, 07eh, 018h, 07eh, 03ch, 018h, 0ffh
    db  018h, 03ch, 07eh, 018h, 018h, 018h, 018h, 000h, 018h, 018h, 018h, 018h, 07eh, 03ch, 018h, 000h
    db  000h, 018h, 00ch, 0feh, 00ch, 018h, 000h, 000h, 000h, 030h, 060h, 0feh, 060h, 030h, 000h, 000h
    db  000h, 000h, 0c0h, 0c0h, 0c0h, 0feh, 000h, 000h, 000h, 024h, 066h, 0ffh, 066h, 024h, 000h, 000h
    db  000h, 018h, 03ch, 07eh, 0ffh, 0ffh, 000h, 000h, 000h, 0ffh, 0ffh, 07eh, 03ch, 018h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 030h, 078h, 078h, 030h, 030h, 000h, 030h, 000h
    db  06ch, 06ch, 06ch, 000h, 000h, 000h, 000h, 000h, 06ch, 06ch, 0feh, 06ch, 0feh, 06ch, 06ch, 000h
    db  030h, 07ch, 0c0h, 078h, 00ch, 0f8h, 030h, 000h, 000h, 0c6h, 0cch, 018h, 030h, 066h, 0c6h, 000h
    db  038h, 06ch, 038h, 076h, 0dch, 0cch, 076h, 000h, 060h, 060h, 0c0h, 000h, 000h, 000h, 000h, 000h
    db  018h, 030h, 060h, 060h, 060h, 030h, 018h, 000h, 060h, 030h, 018h, 018h, 018h, 030h, 060h, 000h
    db  000h, 066h, 03ch, 0ffh, 03ch, 066h, 000h, 000h, 000h, 030h, 030h, 0fch, 030h, 030h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 030h, 030h, 060h, 000h, 000h, 000h, 0fch, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 030h, 030h, 000h, 006h, 00ch, 018h, 030h, 060h, 0c0h, 080h, 000h
    db  07ch, 0c6h, 0ceh, 0deh, 0f6h, 0e6h, 07ch, 000h, 030h, 070h, 030h, 030h, 030h, 030h, 0fch, 000h
    db  078h, 0cch, 00ch, 038h, 060h, 0cch, 0fch, 000h, 078h, 0cch, 00ch, 038h, 00ch, 0cch, 078h, 000h
    db  01ch, 03ch, 06ch, 0cch, 0feh, 00ch, 01eh, 000h, 0fch, 0c0h, 0f8h, 00ch, 00ch, 0cch, 078h, 000h
    db  038h, 060h, 0c0h, 0f8h, 0cch, 0cch, 078h, 000h, 0fch, 0cch, 00ch, 018h, 030h, 030h, 030h, 000h
    db  078h, 0cch, 0cch, 078h, 0cch, 0cch, 078h, 000h, 078h, 0cch, 0cch, 07ch, 00ch, 018h, 070h, 000h
    db  000h, 030h, 030h, 000h, 000h, 030h, 030h, 000h, 000h, 030h, 030h, 000h, 000h, 030h, 030h, 060h
    db  018h, 030h, 060h, 0c0h, 060h, 030h, 018h, 000h, 000h, 000h, 0fch, 000h, 000h, 0fch, 000h, 000h
    db  060h, 030h, 018h, 00ch, 018h, 030h, 060h, 000h, 078h, 0cch, 00ch, 018h, 030h, 000h, 030h, 000h
    db  07ch, 0c6h, 0deh, 0deh, 0deh, 0c0h, 078h, 000h, 030h, 078h, 0cch, 0cch, 0fch, 0cch, 0cch, 000h
    db  0fch, 066h, 066h, 07ch, 066h, 066h, 0fch, 000h, 03ch, 066h, 0c0h, 0c0h, 0c0h, 066h, 03ch, 000h
    db  0f8h, 06ch, 066h, 066h, 066h, 06ch, 0f8h, 000h, 0feh, 062h, 068h, 078h, 068h, 062h, 0feh, 000h
    db  0feh, 062h, 068h, 078h, 068h, 060h, 0f0h, 000h, 03ch, 066h, 0c0h, 0c0h, 0ceh, 066h, 03eh, 000h
    db  0cch, 0cch, 0cch, 0fch, 0cch, 0cch, 0cch, 000h, 078h, 030h, 030h, 030h, 030h, 030h, 078h, 000h
    db  01eh, 00ch, 00ch, 00ch, 0cch, 0cch, 078h, 000h, 0e6h, 066h, 06ch, 078h, 06ch, 066h, 0e6h, 000h
    db  0f0h, 060h, 060h, 060h, 062h, 066h, 0feh, 000h, 0c6h, 0eeh, 0feh, 0feh, 0d6h, 0c6h, 0c6h, 000h
    db  0c6h, 0e6h, 0f6h, 0deh, 0ceh, 0c6h, 0c6h, 000h, 038h, 06ch, 0c6h, 0c6h, 0c6h, 06ch, 038h, 000h
    db  0fch, 066h, 066h, 07ch, 060h, 060h, 0f0h, 000h, 078h, 0cch, 0cch, 0cch, 0dch, 078h, 01ch, 000h
    db  0fch, 066h, 066h, 07ch, 06ch, 066h, 0e6h, 000h, 078h, 0cch, 0e0h, 070h, 01ch, 0cch, 078h, 000h
    db  0fch, 0b4h, 030h, 030h, 030h, 030h, 078h, 000h, 0cch, 0cch, 0cch, 0cch, 0cch, 0cch, 0fch, 000h
    db  0cch, 0cch, 0cch, 0cch, 0cch, 078h, 030h, 000h, 0c6h, 0c6h, 0c6h, 0d6h, 0feh, 0eeh, 0c6h, 000h
    db  0c6h, 0c6h, 06ch, 038h, 038h, 06ch, 0c6h, 000h, 0cch, 0cch, 0cch, 078h, 030h, 030h, 078h, 000h
    db  0feh, 0c6h, 08ch, 018h, 032h, 066h, 0feh, 000h, 078h, 060h, 060h, 060h, 060h, 060h, 078h, 000h
    db  0c0h, 060h, 030h, 018h, 00ch, 006h, 002h, 000h, 078h, 018h, 018h, 018h, 018h, 018h, 078h, 000h
    db  010h, 038h, 06ch, 0c6h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0ffh
    db  030h, 030h, 018h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 078h, 00ch, 07ch, 0cch, 076h, 000h
    db  0e0h, 060h, 060h, 07ch, 066h, 066h, 0dch, 000h, 000h, 000h, 078h, 0cch, 0c0h, 0cch, 078h, 000h
    db  01ch, 00ch, 00ch, 07ch, 0cch, 0cch, 076h, 000h, 000h, 000h, 078h, 0cch, 0fch, 0c0h, 078h, 000h
    db  038h, 06ch, 060h, 0f0h, 060h, 060h, 0f0h, 000h, 000h, 000h, 076h, 0cch, 0cch, 07ch, 00ch, 0f8h
    db  0e0h, 060h, 06ch, 076h, 066h, 066h, 0e6h, 000h, 030h, 000h, 070h, 030h, 030h, 030h, 078h, 000h
    db  00ch, 000h, 00ch, 00ch, 00ch, 0cch, 0cch, 078h, 0e0h, 060h, 066h, 06ch, 078h, 06ch, 0e6h, 000h
    db  070h, 030h, 030h, 030h, 030h, 030h, 078h, 000h, 000h, 000h, 0cch, 0feh, 0feh, 0d6h, 0c6h, 000h
    db  000h, 000h, 0f8h, 0cch, 0cch, 0cch, 0cch, 000h, 000h, 000h, 078h, 0cch, 0cch, 0cch, 078h, 000h
    db  000h, 000h, 0dch, 066h, 066h, 07ch, 060h, 0f0h, 000h, 000h, 076h, 0cch, 0cch, 07ch, 00ch, 01eh
    db  000h, 000h, 0dch, 076h, 066h, 060h, 0f0h, 000h, 000h, 000h, 07ch, 0c0h, 078h, 00ch, 0f8h, 000h
    db  010h, 030h, 07ch, 030h, 030h, 034h, 018h, 000h, 000h, 000h, 0cch, 0cch, 0cch, 0cch, 076h, 000h
    db  000h, 000h, 0cch, 0cch, 0cch, 078h, 030h, 000h, 000h, 000h, 0c6h, 0d6h, 0feh, 0feh, 06ch, 000h
    db  000h, 000h, 0c6h, 06ch, 038h, 06ch, 0c6h, 000h, 000h, 000h, 0cch, 0cch, 0cch, 07ch, 00ch, 0f8h
    db  000h, 000h, 0fch, 098h, 030h, 064h, 0fch, 000h, 01ch, 030h, 030h, 0e0h, 030h, 030h, 01ch, 000h
    db  018h, 018h, 018h, 000h, 018h, 018h, 018h, 000h, 0e0h, 030h, 030h, 01ch, 030h, 030h, 0e0h, 000h
    db  076h, 0dch, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 010h, 038h, 06ch, 0c6h, 0c6h, 0feh, 000h
    db  080h, 0fch, 0b1h, 075h, 00fh, 006h, 01eh, 00eh, 01fh, 0fch, 066h, 060h, 0e8h, 000h, 08eh, 066h
    db  061h, 01fh, 007h, 0cfh, 006h, 01eh, 060h, 00eh, 01fh, 0fch, 0e8h, 095h, 068h, 061h, 01fh, 007h
    db  0cfh
int70_handler:                               ; 0xffe8f LB 0x16
    push ES                                   ; 06
    push DS                                   ; 1e
    pushaw                                    ; 60
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    call 06665h                               ; e8 cd 67
    popaw                                     ; 61
    pop DS                                    ; 1f
    pop ES                                    ; 07
    iret                                      ; cf
    add byte [bx+si], al                      ; 00 00
    add byte [bx+si], al                      ; 00 00
    add byte [bx+si], al                      ; 00 00
    add byte [bx+si+04dh], bl                 ; 00 58 4d
int08_handler:                               ; 0xffea5 LB 0xae
    sti                                       ; fb
    push eax                                  ; 66 50
    push DS                                   ; 1e
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    mov ds, ax                                ; 8e d8
    mov AL, byte [00440h]                     ; a0 40 04
    db  00ah, 0c0h
    ; or al, al                                 ; 0a c0
    je short 0febdh                           ; 74 09
    push dx                                   ; 52
    mov dx, 003f2h                            ; ba f2 03
    in AL, DX                                 ; ec
    and AL, strict byte 0cfh                  ; 24 cf
    out DX, AL                                ; ee
    pop dx                                    ; 5a
    mov eax, dword [0046ch]                   ; 66 a1 6c 04
    inc eax                                   ; 66 40
    cmp eax, strict dword 0001800b0h          ; 66 3d b0 00 18 00
    jc short 0fed2h                           ; 72 07
    db  066h, 033h, 0c0h
    ; xor eax, eax                              ; 66 33 c0
    inc byte [00470h]                         ; fe 06 70 04
    mov dword [0046ch], eax                   ; 66 a3 6c 04
    int 01ch                                  ; cd 1c
    cli                                       ; fa
    call 0e03fh                               ; e8 63 e1
    pop DS                                    ; 1f
    pop eax                                   ; 66 58
    iret                                      ; cf
    times 0x11 db 0
    pop ax                                    ; 58
    dec bp                                    ; 4d
    times 0xb db 0
    pop ax                                    ; 58
    dec bp                                    ; 4d
    dec di                                    ; 4f
    jc short 0ff64h                           ; 72 61
    arpl word [si+065h], bp                   ; 63 6c 65
    and byte [bp+04dh], dl                    ; 20 56 4d
    and byte [bp+069h], dl                    ; 20 56 69
    jc short 0ff82h                           ; 72 74
    jne short 0ff71h                          ; 75 61
    insb                                      ; 6c
    inc dx                                    ; 42
    outsw                                     ; 6f
    js short 0ff35h                           ; 78 20
    inc dx                                    ; 42
    dec cx                                    ; 49
    dec di                                    ; 4f
    push bx                                   ; 53
    times 0x38 db 0
    db  'XM'
dummy_iret:                                  ; 0xfff53 LB 0x9d
    iret                                      ; cf
    iret                                      ; cf
    mov ax, ax                                ; 89 c0
    mov ax, ax                                ; 89 c0
    mov ax, ax                                ; 89 c0
    mov ax, ax                                ; 89 c0
    mov ax, ax                                ; 89 c0
    cld                                       ; fc
    pop di                                    ; 5f
    push bx                                   ; 53
    dec bp                                    ; 4d
    pop di                                    ; 5f
    jnl short 0ff85h                          ; 7d 1f
    add al, byte [di]                         ; 02 05
    inc word [bx+si]                          ; ff 00
    add byte [bx+si], al                      ; 00 00
    add byte [bx+si], al                      ; 00 00
    add byte [bx+si], al                      ; 00 00
    pop di                                    ; 5f
    inc sp                                    ; 44
    dec bp                                    ; 4d
    dec cx                                    ; 49
    pop di                                    ; 5f
    sbb AL, strict byte 000h                  ; 1c 00
    add byte [bx+si], al                      ; 00 00
    adc byte [00900h], cl                     ; 10 0e 00 09
    add byte [di], ah                         ; 00 25
    times 0x6f db 0
    db  'XM'
cpu_reset:                                   ; 0xffff0 LB 0x10
    jmp far 0f000h:0e05bh                     ; ea 5b e0 00 f0
    db  030h, 036h, 02fh, 032h, 033h, 02fh, 039h, 039h, 000h, 0fch, 039h
