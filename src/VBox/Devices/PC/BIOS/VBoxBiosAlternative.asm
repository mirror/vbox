; $Id$ 
;; @file
; Auto Generated source file. Do not edit.
;

;
; Source file: post.c
;
;  BIOS POST routines. Used only during initialization.
;  
;  
;  
;  Copyright (C) 2004-2014 Oracle Corporation
;  
;  This file is part of VirtualBox Open Source Edition (OSE), as
;  available from http://www.virtualbox.org. This file is free software;
;  you can redistribute it and/or modify it under the terms of the GNU
;  General Public License (GPL) as published by the Free Software
;  Foundation, in version 2 as it comes in the "COPYING" file of the
;  VirtualBox OSE distribution. VirtualBox OSE is distributed in the
;  hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.

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
; Source file: floppyt.c
;
;  $Id$
;  Floppy drive tables.
;  
;  
;  
;  Copyright (C) 2011-2013 Oracle Corporation
;  
;  This file is part of VirtualBox Open Source Edition (OSE), as
;  available from http://www.virtualbox.org. This file is free software;
;  you can redistribute it and/or modify it under the terms of the GNU
;  General Public License (GPL) as published by the Free Software
;  Foundation, in version 2 as it comes in the "COPYING" file of the
;  VirtualBox OSE distribution. VirtualBox OSE is distributed in the
;  hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.

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
; Source file: invop.c
;
;  $Id$
;  Real mode invalid opcode handler.
;  
;  
;  
;  Copyright (C) 2013 Oracle Corporation
;  
;  This file is part of VirtualBox Open Source Edition (OSE), as
;  available from http://www.virtualbox.org. This file is free software;
;  you can redistribute it and/or modify it under the terms of the GNU
;  General Public License (GPL) as published by the Free Software
;  Foundation, in version 2 as it comes in the "COPYING" file of the
;  VirtualBox OSE distribution. VirtualBox OSE is distributed in the
;  hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.

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




section _DATA progbits vstart=0x0 align=1 ; size=0xb0 class=DATA group=DGROUP
_fd_parm:                                    ; 0xf0000 LB 0x5b
    db  0dfh, 002h, 025h, 002h, 009h, 02ah, 0ffh, 050h, 0f6h, 00fh, 008h, 027h, 080h, 0dfh, 002h, 025h
    db  002h, 009h, 02ah, 0ffh, 050h, 0f6h, 00fh, 008h, 027h, 040h, 0dfh, 002h, 025h, 002h, 00fh, 01bh
    db  0ffh, 054h, 0f6h, 00fh, 008h, 04fh, 000h, 0dfh, 002h, 025h, 002h, 009h, 02ah, 0ffh, 050h, 0f6h
    db  00fh, 008h, 04fh, 080h, 0afh, 002h, 025h, 002h, 012h, 01bh, 0ffh, 06ch, 0f6h, 00fh, 008h, 04fh
    db  000h, 0afh, 002h, 025h, 002h, 024h, 01bh, 0ffh, 054h, 0f6h, 00fh, 008h, 04fh, 0c0h, 0afh, 002h
    db  025h, 002h, 0ffh, 01bh, 0ffh, 054h, 0f6h, 00fh, 008h, 0ffh, 000h
_fd_map:                                     ; 0xf005b LB 0xf
    db  001h, 000h, 002h, 002h, 003h, 003h, 004h, 004h, 005h, 005h, 00eh, 006h, 00fh, 006h, 000h
_pktacc:                                     ; 0xf006a LB 0xc
    db  000h, 000h, 000h, 000h, 000h, 000h, 08fh, 028h, 030h, 079h, 0f8h, 086h
_softrst:                                    ; 0xf0076 LB 0xc
    db  000h, 000h, 000h, 000h, 000h, 000h, 077h, 02bh, 091h, 036h, 091h, 036h
_dskacc:                                     ; 0xf0082 LB 0x2e
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0d8h, 027h, 052h, 028h, 000h, 000h, 000h, 000h
    db  0beh, 077h, 077h, 078h, 0f4h, 085h, 088h, 086h, 000h, 000h, 000h, 000h, 000h, 000h, 05fh, 033h
    db  032h, 05fh, 000h, 0dah, 00fh, 000h, 000h, 001h, 0f3h, 000h, 000h, 000h, 000h, 000h

section CONST progbits vstart=0xb0 align=1 ; size=0xcc8 class=DATA group=DGROUP
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
    db   'scsi_read_sectors', 000h
    db   '%s: device_id out of range %d', 00ah, 000h
    db   'scsi_write_sectors', 000h
    db   'scsi_cmd_packet', 000h
    db   '%s: DATA_OUT not supported yet', 00ah, 000h
    db   'scsi_enumerate_attached_devices', 000h
    db   '%s: SCSI_INQUIRY failed', 00ah, 000h
    db   '%s: SCSI_READ_CAPACITY failed', 00ah, 000h
    db   'Disk %d has an unsupported sector size of %u', 00ah, 000h
    db   'SCSI %d-ID#%d: LCHS=%u/%u/%u %ld sectors', 00ah, 000h
    db   'SCSI %d-ID#%d: CD/DVD-ROM', 00ah, 000h, 000h
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

section CONST2 progbits vstart=0xd78 align=1 ; size=0x3fa class=DATA group=DGROUP
_bios_cvs_version_string:                    ; 0xf0d78 LB 0x12
    db  'VirtualBox 4.3.53', 000h
_bios_prefix_string:                         ; 0xf0d8a LB 0x8
    db  'BIOS: ', 000h, 000h
_isotag:                                     ; 0xf0d92 LB 0x6
    db  'CD001', 000h
_eltorito:                                   ; 0xf0d98 LB 0x18
    db  'EL TORITO SPECIFICATION', 000h
_drivetypes:                                 ; 0xf0db0 LB 0x28
    db  046h, 06ch, 06fh, 070h, 070h, 079h, 000h, 000h, 000h, 000h, 048h, 061h, 072h, 064h, 020h, 044h
    db  069h, 073h, 06bh, 000h, 043h, 044h, 02dh, 052h, 04fh, 04dh, 000h, 000h, 000h, 000h, 04ch, 041h
    db  04eh, 000h, 000h, 000h, 000h, 000h, 000h, 000h
_scan_to_scanascii:                          ; 0xf0dd8 LB 0x37a
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
_panic_msg_keyb_buffer_full:                 ; 0xf1152 LB 0x20
    db  '%s: keyboard input buffer full', 00ah, 000h

  ; Padding 0x48e bytes at 0xf1172
  times 1166 db 0

section _TEXT progbits vstart=0x1600 align=1 ; size=0x7f65 class=CODE group=AUTO
rom_scan_:                                   ; 0xf1600 LB 0x50
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    push cx                                   ; 51
    push si                                   ; 56
    push ax                                   ; 50
    push ax                                   ; 50
    mov bx, ax                                ; 89 c3
    mov cx, dx                                ; 89 d1
    cmp bx, cx                                ; 39 cb
    jnc short 01648h                          ; 73 38
    xor si, si                                ; 31 f6
    mov dx, bx                                ; 89 da
    mov es, bx                                ; 8e c3
    cmp word [es:si], 0aa55h                  ; 26 81 3c 55 aa
    jne short 01642h                          ; 75 25
    mov word [bp-008h], bx                    ; 89 5e f8
    mov word [bp-00ah], strict word 00003h    ; c7 46 f6 03 00
    call far [bp-00ah]                        ; ff 5e f6
    cli                                       ; fa
    mov es, bx                                ; 8e c3
    mov al, byte [es:si+002h]                 ; 26 8a 44 02
    add AL, strict byte 003h                  ; 04 03
    and AL, strict byte 0fch                  ; 24 fc
    xor ah, ah                                ; 30 e4
    cwd                                       ; 99
    sal dx, 002h                              ; c1 e2 02
    db  01bh, 0c2h
    ; sbb ax, dx                                ; 1b c2
    sar ax, 002h                              ; c1 f8 02
    add bx, ax                                ; 01 c3
    jmp short 0160ch                          ; eb ca
    add bx, 00080h                            ; 81 c3 80 00
    jmp short 0160ch                          ; eb c4
    lea sp, [bp-006h]                         ; 8d 66 fa
    pop si                                    ; 5e
    pop cx                                    ; 59
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
read_byte_:                                  ; 0xf1650 LB 0xe
    push bx                                   ; 53
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, dx                                ; 89 d3
    mov es, ax                                ; 8e c0
    mov al, byte [es:bx]                      ; 26 8a 07
    pop bp                                    ; 5d
    pop bx                                    ; 5b
    retn                                      ; c3
write_byte_:                                 ; 0xf165e LB 0xe
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov si, dx                                ; 89 d6
    mov es, ax                                ; 8e c0
    mov byte [es:si], bl                      ; 26 88 1c
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn                                      ; c3
read_word_:                                  ; 0xf166c LB 0xe
    push bx                                   ; 53
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, dx                                ; 89 d3
    mov es, ax                                ; 8e c0
    mov ax, word [es:bx]                      ; 26 8b 07
    pop bp                                    ; 5d
    pop bx                                    ; 5b
    retn                                      ; c3
write_word_:                                 ; 0xf167a LB 0xe
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov si, dx                                ; 89 d6
    mov es, ax                                ; 8e c0
    mov word [es:si], bx                      ; 26 89 1c
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn                                      ; c3
read_dword_:                                 ; 0xf1688 LB 0x12
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
write_dword_:                                ; 0xf169a LB 0x12
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
inb_cmos_:                                   ; 0xf16ac LB 0x1d
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push dx                                   ; 52
    mov AH, strict byte 070h                  ; b4 70
    cmp AL, strict byte 080h                  ; 3c 80
    jc short 016b8h                           ; 72 02
    mov AH, strict byte 072h                  ; b4 72
    movzx dx, ah                              ; 0f b6 d4
    out DX, AL                                ; ee
    movzx dx, ah                              ; 0f b6 d4
    inc dx                                    ; 42
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    lea sp, [bp-002h]                         ; 8d 66 fe
    pop dx                                    ; 5a
    pop bp                                    ; 5d
    retn                                      ; c3
outb_cmos_:                                  ; 0xf16c9 LB 0x1f
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    mov ah, dl                                ; 88 d4
    mov BL, strict byte 070h                  ; b3 70
    cmp AL, strict byte 080h                  ; 3c 80
    jc short 016d7h                           ; 72 02
    mov BL, strict byte 072h                  ; b3 72
    movzx dx, bl                              ; 0f b6 d3
    out DX, AL                                ; ee
    movzx dx, bl                              ; 0f b6 d3
    inc dx                                    ; 42
    mov al, ah                                ; 88 e0
    out DX, AL                                ; ee
    lea sp, [bp-002h]                         ; 8d 66 fe
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
_dummy_isr_function:                         ; 0xf16e8 LB 0x6b
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push ax                                   ; 50
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
    je short 01743h                           ; 74 43
    mov AL, strict byte 00bh                  ; b0 0b
    mov dx, 000a0h                            ; ba a0 00
    out DX, AL                                ; ee
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov cx, ax                                ; 89 c1
    test al, al                               ; 84 c0
    je short 01725h                           ; 74 16
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
    jmp short 0173ah                          ; eb 15
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
    call 0165eh                               ; e8 0f ff
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    retn                                      ; c3
_nmi_handler_msg:                            ; 0xf1753 LB 0x12
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push 000b0h                               ; 68 b0 00
    push strict byte 00007h                   ; 6a 07
    call 01972h                               ; e8 14 02
    add sp, strict byte 00004h                ; 83 c4 04
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    retn                                      ; c3
_int18_panic_msg:                            ; 0xf1765 LB 0x12
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push 000c4h                               ; 68 c4 00
    push strict byte 00007h                   ; 6a 07
    call 01972h                               ; e8 02 02
    add sp, strict byte 00004h                ; 83 c4 04
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    retn                                      ; c3
_log_bios_start:                             ; 0xf1777 LB 0x20
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 ac 01
    push 00d78h                               ; 68 78 0d
    push 000d9h                               ; 68 d9 00
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 e2 01
    add sp, strict byte 00006h                ; 83 c4 06
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    retn                                      ; c3
_print_bios_banner:                          ; 0xf1797 LB 0x2e
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, strict word 00072h                ; ba 72 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 c9 fe
    mov cx, ax                                ; 89 c1
    xor bx, bx                                ; 31 db
    mov dx, strict word 00072h                ; ba 72 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0167ah                               ; e8 ca fe
    cmp cx, 01234h                            ; 81 f9 34 12
    jne short 017beh                          ; 75 08
    mov AL, strict byte 003h                  ; b0 03
    mov AH, strict byte 000h                  ; b4 00
    int 010h                                  ; cd 10
    jmp short 017c1h                          ; eb 03
    call 073cdh                               ; e8 0c 5c
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    retn                                      ; c3
send_:                                       ; 0xf17c5 LB 0x3b
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    push cx                                   ; 51
    mov bx, ax                                ; 89 c3
    mov cl, dl                                ; 88 d1
    test AL, strict byte 008h                 ; a8 08
    je short 017d8h                           ; 74 06
    mov al, dl                                ; 88 d0
    mov dx, 00403h                            ; ba 03 04
    out DX, AL                                ; ee
    test bl, 004h                             ; f6 c3 04
    je short 017e3h                           ; 74 06
    mov al, cl                                ; 88 c8
    mov dx, 00504h                            ; ba 04 05
    out DX, AL                                ; ee
    test bl, 002h                             ; f6 c3 02
    je short 017f9h                           ; 74 11
    cmp cl, 00ah                              ; 80 f9 0a
    jne short 017f3h                          ; 75 06
    mov AL, strict byte 00dh                  ; b0 0d
    mov AH, strict byte 00eh                  ; b4 0e
    int 010h                                  ; cd 10
    mov al, cl                                ; 88 c8
    mov AH, strict byte 00eh                  ; b4 0e
    int 010h                                  ; cd 10
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop cx                                    ; 59
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
put_int_:                                    ; 0xf1800 LB 0x5f
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    push di                                   ; 57
    push ax                                   ; 50
    push ax                                   ; 50
    mov si, ax                                ; 89 c6
    mov word [bp-008h], dx                    ; 89 56 f8
    mov di, strict word 0000ah                ; bf 0a 00
    mov ax, dx                                ; 89 d0
    cwd                                       ; 99
    idiv di                                   ; f7 ff
    mov word [bp-006h], ax                    ; 89 46 fa
    test ax, ax                               ; 85 c0
    je short 01825h                           ; 74 0a
    dec bx                                    ; 4b
    mov dx, ax                                ; 89 c2
    mov ax, si                                ; 89 f0
    call 01800h                               ; e8 dd ff
    jmp short 01840h                          ; eb 1b
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jle short 01834h                          ; 7e 0a
    mov dx, strict word 00020h                ; ba 20 00
    mov ax, si                                ; 89 f0
    call 017c5h                               ; e8 93 ff
    jmp short 01825h                          ; eb f1
    test cx, cx                               ; 85 c9
    je short 01840h                           ; 74 08
    mov dx, strict word 0002dh                ; ba 2d 00
    mov ax, si                                ; 89 f0
    call 017c5h                               ; e8 85 ff
    mov al, byte [bp-006h]                    ; 8a 46 fa
    mov DL, strict byte 00ah                  ; b2 0a
    mul dl                                    ; f6 e2
    mov dl, byte [bp-008h]                    ; 8a 56 f8
    sub dl, al                                ; 28 c2
    mov al, dl                                ; 88 d0
    add AL, strict byte 030h                  ; 04 30
    movzx dx, al                              ; 0f b6 d0
    mov ax, si                                ; 89 f0
    call 017c5h                               ; e8 6d ff
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn                                      ; c3
put_uint_:                                   ; 0xf185f LB 0x60
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    push di                                   ; 57
    push ax                                   ; 50
    push ax                                   ; 50
    mov si, ax                                ; 89 c6
    mov word [bp-008h], dx                    ; 89 56 f8
    mov ax, dx                                ; 89 d0
    xor dx, dx                                ; 31 d2
    mov di, strict word 0000ah                ; bf 0a 00
    div di                                    ; f7 f7
    mov word [bp-006h], ax                    ; 89 46 fa
    test ax, ax                               ; 85 c0
    je short 01885h                           ; 74 0a
    dec bx                                    ; 4b
    mov dx, ax                                ; 89 c2
    mov ax, si                                ; 89 f0
    call 0185fh                               ; e8 dc ff
    jmp short 018a0h                          ; eb 1b
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jle short 01894h                          ; 7e 0a
    mov dx, strict word 00020h                ; ba 20 00
    mov ax, si                                ; 89 f0
    call 017c5h                               ; e8 33 ff
    jmp short 01885h                          ; eb f1
    test cx, cx                               ; 85 c9
    je short 018a0h                           ; 74 08
    mov dx, strict word 0002dh                ; ba 2d 00
    mov ax, si                                ; 89 f0
    call 017c5h                               ; e8 25 ff
    mov al, byte [bp-006h]                    ; 8a 46 fa
    mov DL, strict byte 00ah                  ; b2 0a
    mul dl                                    ; f6 e2
    mov dl, byte [bp-008h]                    ; 8a 56 f8
    sub dl, al                                ; 28 c2
    mov al, dl                                ; 88 d0
    add AL, strict byte 030h                  ; 04 30
    movzx dx, al                              ; 0f b6 d0
    mov ax, si                                ; 89 f0
    call 017c5h                               ; e8 0d ff
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn                                      ; c3
put_luint_:                                  ; 0xf18bf LB 0x72
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    push di                                   ; 57
    push ax                                   ; 50
    push ax                                   ; 50
    mov si, ax                                ; 89 c6
    mov word [bp-006h], bx                    ; 89 5e fa
    mov di, dx                                ; 89 d7
    mov ax, bx                                ; 89 d8
    mov dx, cx                                ; 89 ca
    mov bx, strict word 0000ah                ; bb 0a 00
    xor cx, cx                                ; 31 c9
    call 09470h                               ; e8 97 7b
    mov word [bp-008h], ax                    ; 89 46 f8
    mov cx, dx                                ; 89 d1
    mov dx, ax                                ; 89 c2
    or dx, cx                                 ; 09 ca
    je short 018f3h                           ; 74 0f
    push word [bp+004h]                       ; ff 76 04
    lea dx, [di-001h]                         ; 8d 55 ff
    mov bx, ax                                ; 89 c3
    mov ax, si                                ; 89 f0
    call 018bfh                               ; e8 ce ff
    jmp short 01910h                          ; eb 1d
    dec di                                    ; 4f
    test di, di                               ; 85 ff
    jle short 01902h                          ; 7e 0a
    mov dx, strict word 00020h                ; ba 20 00
    mov ax, si                                ; 89 f0
    call 017c5h                               ; e8 c5 fe
    jmp short 018f3h                          ; eb f1
    cmp word [bp+004h], strict byte 00000h    ; 83 7e 04 00
    je short 01910h                           ; 74 08
    mov dx, strict word 0002dh                ; ba 2d 00
    mov ax, si                                ; 89 f0
    call 017c5h                               ; e8 b5 fe
    mov al, byte [bp-008h]                    ; 8a 46 f8
    mov DL, strict byte 00ah                  ; b2 0a
    mul dl                                    ; f6 e2
    mov dl, byte [bp-006h]                    ; 8a 56 fa
    sub dl, al                                ; 28 c2
    mov al, dl                                ; 88 d0
    add AL, strict byte 030h                  ; 04 30
    movzx dx, al                              ; 0f b6 d0
    mov ax, si                                ; 89 f0
    call 017c5h                               ; e8 9d fe
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn 00002h                               ; c2 02 00
put_str_:                                    ; 0xf1931 LB 0x21
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push dx                                   ; 52
    push si                                   ; 56
    mov si, ax                                ; 89 c6
    mov es, cx                                ; 8e c1
    mov dl, byte [es:bx]                      ; 26 8a 17
    test dl, dl                               ; 84 d2
    je short 0194bh                           ; 74 0a
    xor dh, dh                                ; 30 f6
    mov ax, si                                ; 89 f0
    call 017c5h                               ; e8 7d fe
    inc bx                                    ; 43
    jmp short 01938h                          ; eb ed
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop bp                                    ; 5d
    retn                                      ; c3
put_str_near_:                               ; 0xf1952 LB 0x20
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    push cx                                   ; 51
    mov cx, ax                                ; 89 c1
    mov bx, dx                                ; 89 d3
    mov dl, byte [bx]                         ; 8a 17
    test dl, dl                               ; 84 d2
    je short 0196bh                           ; 74 0a
    xor dh, dh                                ; 30 f6
    mov ax, cx                                ; 89 c8
    call 017c5h                               ; e8 5d fe
    inc bx                                    ; 43
    jmp short 0195bh                          ; eb f0
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop cx                                    ; 59
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
bios_printf_:                                ; 0xf1972 LB 0x23b
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push si                                   ; 56
    push di                                   ; 57
    sub sp, strict byte 00008h                ; 83 ec 08
    lea bx, [bp+008h]                         ; 8d 5e 08
    mov word [bp-012h], bx                    ; 89 5e ee
    mov [bp-010h], ss                         ; 8c 56 f0
    xor cx, cx                                ; 31 c9
    xor si, si                                ; 31 f6
    mov ax, word [bp+004h]                    ; 8b 46 04
    and ax, strict word 00007h                ; 25 07 00
    cmp ax, strict word 00007h                ; 3d 07 00
    jne short 019a0h                          ; 75 0b
    push 000deh                               ; 68 de 00
    push strict byte 00002h                   ; 6a 02
    call 01972h                               ; e8 d5 ff
    add sp, strict byte 00004h                ; 83 c4 04
    mov bx, word [bp+006h]                    ; 8b 5e 06
    mov dl, byte [bx]                         ; 8a 17
    test dl, dl                               ; 84 d2
    je near 01b91h                            ; 0f 84 e6 01
    cmp dl, 025h                              ; 80 fa 25
    jne short 019b8h                          ; 75 08
    mov cx, strict word 00001h                ; b9 01 00
    xor si, si                                ; 31 f6
    jmp near 01b8bh                           ; e9 d3 01
    test cx, cx                               ; 85 c9
    je near 01b83h                            ; 0f 84 c5 01
    cmp dl, 030h                              ; 80 fa 30
    jc short 019d6h                           ; 72 13
    cmp dl, 039h                              ; 80 fa 39
    jnbe short 019d6h                         ; 77 0e
    movzx ax, dl                              ; 0f b6 c2
    imul si, si, strict byte 0000ah           ; 6b f6 0a
    sub ax, strict word 00030h                ; 2d 30 00
    add si, ax                                ; 01 c6
    jmp near 01b8bh                           ; e9 b5 01
    mov ax, word [bp-010h]                    ; 8b 46 f0
    mov word [bp-010h], ax                    ; 89 46 f0
    add word [bp-012h], strict byte 00002h    ; 83 46 ee 02
    les bx, [bp-012h]                         ; c4 5e ee
    mov ax, word [es:bx-002h]                 ; 26 8b 47 fe
    mov word [bp-00ch], ax                    ; 89 46 f4
    cmp dl, 078h                              ; 80 fa 78
    je short 019f4h                           ; 74 05
    cmp dl, 058h                              ; 80 fa 58
    jne short 01a3dh                          ; 75 49
    test si, si                               ; 85 f6
    jne short 019fbh                          ; 75 03
    mov si, strict word 00004h                ; be 04 00
    cmp dl, 078h                              ; 80 fa 78
    jne short 01a05h                          ; 75 05
    mov di, strict word 00061h                ; bf 61 00
    jmp short 01a08h                          ; eb 03
    mov di, strict word 00041h                ; bf 41 00
    lea bx, [si-001h]                         ; 8d 5c ff
    test bx, bx                               ; 85 db
    jl near 01b7fh                            ; 0f 8c 6e 01
    mov cx, bx                                ; 89 d9
    sal cx, 002h                              ; c1 e1 02
    mov ax, word [bp-00ch]                    ; 8b 46 f4
    shr ax, CL                                ; d3 e8
    xor ah, ah                                ; 30 e4
    and AL, strict byte 00fh                  ; 24 0f
    cmp ax, strict word 00009h                ; 3d 09 00
    jnbe short 01a2bh                         ; 77 07
    mov dx, ax                                ; 89 c2
    add dx, strict byte 00030h                ; 83 c2 30
    jmp short 01a32h                          ; eb 07
    mov dx, ax                                ; 89 c2
    sub dx, strict byte 0000ah                ; 83 ea 0a
    add dx, di                                ; 01 fa
    xor dh, dh                                ; 30 f6
    mov ax, word [bp+004h]                    ; 8b 46 04
    call 017c5h                               ; e8 8b fd
    dec bx                                    ; 4b
    jmp short 01a0bh                          ; eb ce
    cmp dl, 075h                              ; 80 fa 75
    jne short 01a51h                          ; 75 0f
    xor cx, cx                                ; 31 c9
    mov bx, si                                ; 89 f3
    mov dx, ax                                ; 89 c2
    mov ax, word [bp+004h]                    ; 8b 46 04
    call 0185fh                               ; e8 11 fe
    jmp near 01b7fh                           ; e9 2e 01
    lea bx, [si-001h]                         ; 8d 5c ff
    cmp dl, 06ch                              ; 80 fa 6c
    jne near 01b0dh                           ; 0f 85 b2 00
    inc word [bp+006h]                        ; ff 46 06
    mov di, word [bp+006h]                    ; 8b 7e 06
    mov dl, byte [di]                         ; 8a 15
    mov ax, word [bp-010h]                    ; 8b 46 f0
    mov word [bp-010h], ax                    ; 89 46 f0
    add word [bp-012h], strict byte 00002h    ; 83 46 ee 02
    les di, [bp-012h]                         ; c4 7e ee
    mov ax, word [es:di-002h]                 ; 26 8b 45 fe
    mov word [bp-00eh], ax                    ; 89 46 f2
    cmp dl, 064h                              ; 80 fa 64
    jne short 01aa9h                          ; 75 2d
    test byte [bp-00dh], 080h                 ; f6 46 f3 80
    je short 01a97h                           ; 74 15
    push strict byte 00001h                   ; 6a 01
    mov ax, word [bp-00ch]                    ; 8b 46 f4
    mov cx, word [bp-00eh]                    ; 8b 4e f2
    neg cx                                    ; f7 d9
    neg ax                                    ; f7 d8
    sbb cx, strict byte 00000h                ; 83 d9 00
    mov dx, bx                                ; 89 da
    mov bx, ax                                ; 89 c3
    jmp short 01aa0h                          ; eb 09
    push strict byte 00000h                   ; 6a 00
    mov bx, word [bp-00ch]                    ; 8b 5e f4
    mov dx, si                                ; 89 f2
    mov cx, ax                                ; 89 c1
    mov ax, word [bp+004h]                    ; 8b 46 04
    call 018bfh                               ; e8 19 fe
    jmp near 01b7fh                           ; e9 d6 00
    cmp dl, 075h                              ; 80 fa 75
    jne short 01ab0h                          ; 75 02
    jmp short 01a97h                          ; eb e7
    cmp dl, 078h                              ; 80 fa 78
    je short 01abch                           ; 74 07
    cmp dl, 058h                              ; 80 fa 58
    jne near 01b7fh                           ; 0f 85 c3 00
    test si, si                               ; 85 f6
    jne short 01ac3h                          ; 75 03
    mov si, strict word 00008h                ; be 08 00
    cmp dl, 078h                              ; 80 fa 78
    jne short 01acdh                          ; 75 05
    mov di, strict word 00061h                ; bf 61 00
    jmp short 01ad0h                          ; eb 03
    mov di, strict word 00041h                ; bf 41 00
    lea bx, [si-001h]                         ; 8d 5c ff
    test bx, bx                               ; 85 db
    jl near 01b7fh                            ; 0f 8c a6 00
    mov ax, word [bp-00ch]                    ; 8b 46 f4
    mov cx, bx                                ; 89 d9
    sal cx, 002h                              ; c1 e1 02
    mov dx, word [bp-00eh]                    ; 8b 56 f2
    jcxz 01aech                               ; e3 06
    shr dx, 1                                 ; d1 ea
    rcr ax, 1                                 ; d1 d8
    loop 01ae6h                               ; e2 fa
    and ax, strict word 0000fh                ; 25 0f 00
    cmp ax, strict word 00009h                ; 3d 09 00
    jnbe short 01afbh                         ; 77 07
    mov dx, ax                                ; 89 c2
    add dx, strict byte 00030h                ; 83 c2 30
    jmp short 01b02h                          ; eb 07
    mov dx, ax                                ; 89 c2
    sub dx, strict byte 0000ah                ; 83 ea 0a
    add dx, di                                ; 01 fa
    xor dh, dh                                ; 30 f6
    mov ax, word [bp+004h]                    ; 8b 46 04
    call 017c5h                               ; e8 bb fc
    dec bx                                    ; 4b
    jmp short 01ad3h                          ; eb c6
    cmp dl, 064h                              ; 80 fa 64
    jne short 01b2fh                          ; 75 1d
    test byte [bp-00bh], 080h                 ; f6 46 f5 80
    je short 01b21h                           ; 74 09
    mov dx, ax                                ; 89 c2
    neg dx                                    ; f7 da
    mov cx, strict word 00001h                ; b9 01 00
    jmp short 01b27h                          ; eb 06
    xor cx, cx                                ; 31 c9
    mov bx, si                                ; 89 f3
    mov dx, ax                                ; 89 c2
    mov ax, word [bp+004h]                    ; 8b 46 04
    call 01800h                               ; e8 d3 fc
    jmp short 01b7fh                          ; eb 50
    cmp dl, 073h                              ; 80 fa 73
    jne short 01b40h                          ; 75 0c
    mov cx, ds                                ; 8c d9
    mov bx, ax                                ; 89 c3
    mov ax, word [bp+004h]                    ; 8b 46 04
    call 01931h                               ; e8 f3 fd
    jmp short 01b7fh                          ; eb 3f
    cmp dl, 053h                              ; 80 fa 53
    jne short 01b63h                          ; 75 1e
    mov word [bp-00eh], ax                    ; 89 46 f2
    mov ax, word [bp-010h]                    ; 8b 46 f0
    mov word [bp-010h], ax                    ; 89 46 f0
    add word [bp-012h], strict byte 00002h    ; 83 46 ee 02
    les bx, [bp-012h]                         ; c4 5e ee
    mov ax, word [es:bx-002h]                 ; 26 8b 47 fe
    mov word [bp-00ch], ax                    ; 89 46 f4
    mov bx, ax                                ; 89 c3
    mov cx, word [bp-00eh]                    ; 8b 4e f2
    jmp short 01b38h                          ; eb d5
    cmp dl, 063h                              ; 80 fa 63
    jne short 01b74h                          ; 75 0c
    movzx dx, byte [bp-00ch]                  ; 0f b6 56 f4
    mov ax, word [bp+004h]                    ; 8b 46 04
    call 017c5h                               ; e8 53 fc
    jmp short 01b7fh                          ; eb 0b
    push 000e6h                               ; 68 e6 00
    push strict byte 00007h                   ; 6a 07
    call 01972h                               ; e8 f6 fd
    add sp, strict byte 00004h                ; 83 c4 04
    xor cx, cx                                ; 31 c9
    jmp short 01b8bh                          ; eb 08
    xor dh, dh                                ; 30 f6
    mov ax, word [bp+004h]                    ; 8b 46 04
    call 017c5h                               ; e8 3a fc
    inc word [bp+006h]                        ; ff 46 06
    jmp near 019a0h                           ; e9 0f fe
    xor ax, ax                                ; 31 c0
    mov word [bp-012h], ax                    ; 89 46 ee
    mov word [bp-010h], ax                    ; 89 46 f0
    test byte [bp+004h], 001h                 ; f6 46 04 01
    je short 01ba3h                           ; 74 04
    cli                                       ; fa
    hlt                                       ; f4
    jmp short 01ba0h                          ; eb fd
    lea sp, [bp-00ah]                         ; 8d 66 f6
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
_ata_init:                                   ; 0xf1bad LB 0xc4
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 b2 fa
    mov si, 00122h                            ; be 22 01
    mov dx, ax                                ; 89 c2
    xor al, al                                ; 30 c0
    jmp short 01bc7h                          ; eb 04
    cmp AL, strict byte 004h                  ; 3c 04
    jnc short 01bebh                          ; 73 24
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
    jmp short 01bc3h                          ; eb d8
    xor al, al                                ; 30 c0
    jmp short 01bf3h                          ; eb 04
    cmp AL, strict byte 008h                  ; 3c 08
    jnc short 01c3eh                          ; 73 4b
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
    jmp short 01befh                          ; eb b1
    xor al, al                                ; 30 c0
    jmp short 01c46h                          ; eb 04
    cmp AL, strict byte 010h                  ; 3c 10
    jnc short 01c5dh                          ; 73 17
    movzx bx, al                              ; 0f b6 d8
    mov es, dx                                ; 8e c2
    add bx, si                                ; 01 f3
    mov byte [es:bx+0019fh], 010h             ; 26 c6 87 9f 01 10
    mov byte [es:bx+001b0h], 010h             ; 26 c6 87 b0 01 10
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    jmp short 01c42h                          ; eb e5
    mov es, dx                                ; 8e c2
    mov byte [es:si+0019eh], 000h             ; 26 c6 84 9e 01 00
    mov byte [es:si+001afh], 000h             ; 26 c6 84 af 01 00
    lea sp, [bp-002h]                         ; 8d 66 fe
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn                                      ; c3
ata_reset_:                                  ; 0xf1c71 LB 0xde
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push si                                   ; 56
    push di                                   ; 57
    push ax                                   ; 50
    push ax                                   ; 50
    push ax                                   ; 50
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 e7 f9
    mov word [bp-00eh], 00122h                ; c7 46 f2 22 01
    mov di, ax                                ; 89 c7
    mov bx, word [bp-010h]                    ; 8b 5e f0
    shr bx, 1                                 ; d1 eb
    mov dl, byte [bp-010h]                    ; 8a 56 f0
    and dl, 001h                              ; 80 e2 01
    mov byte [bp-00ch], dl                    ; 88 56 f4
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
    jbe short 01cc9h                          ; 76 0c
    mov dx, cx                                ; 89 ca
    add dx, strict byte 00007h                ; 83 c2 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 080h                 ; a8 80
    je short 01cb8h                           ; 74 ef
    lea dx, [si+006h]                         ; 8d 54 06
    mov AL, strict byte 00ah                  ; b0 0a
    out DX, AL                                ; ee
    imul bx, word [bp-010h], strict byte 00018h ; 6b 5e f0 18
    mov es, di                                ; 8e c7
    add bx, word [bp-00eh]                    ; 03 5e f2
    cmp byte [es:bx+01eh], 000h               ; 26 80 7f 1e 00
    je short 01d2bh                           ; 74 4c
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00
    je short 01ceah                           ; 74 05
    mov ax, 000b0h                            ; b8 b0 00
    jmp short 01cedh                          ; eb 03
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
    jne short 01d2bh                          ; 75 22
    cmp al, bl                                ; 38 d8
    jne short 01d2bh                          ; 75 1e
    mov bx, strict word 0ffffh                ; bb ff ff
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 01d2bh                          ; 76 16
    mov dx, cx                                ; 89 ca
    add dx, strict byte 00007h                ; 83 c2 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 080h                 ; a8 80
    je short 01d2bh                           ; 74 0a
    mov ax, strict word 0ffffh                ; b8 ff ff
    dec ax                                    ; 48
    test ax, ax                               ; 85 c0
    jnbe short 01d24h                         ; 77 fb
    jmp short 01d10h                          ; eb e5
    mov bx, strict word 00010h                ; bb 10 00
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 01d3fh                          ; 76 0c
    mov dx, cx                                ; 89 ca
    add dx, strict byte 00007h                ; 83 c2 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 040h                 ; a8 40
    je short 01d2eh                           ; 74 ef
    lea dx, [si+006h]                         ; 8d 54 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    lea sp, [bp-00ah]                         ; 8d 66 f6
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
ata_cmd_data_in_:                            ; 0xf1d4f LB 0x258
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    push di                                   ; 57
    sub sp, strict byte 0001ch                ; 83 ec 1c
    mov si, ax                                ; 89 c6
    mov word [bp-00ah], dx                    ; 89 56 f6
    mov word [bp-016h], bx                    ; 89 5e ea
    mov word [bp-014h], cx                    ; 89 4e ec
    mov es, dx                                ; 8e c2
    movzx ax, byte [es:si+008h]               ; 26 0f b6 44 08
    mov dx, ax                                ; 89 c2
    shr dx, 1                                 ; d1 ea
    mov dh, al                                ; 88 c6
    and dh, 001h                              ; 80 e6 01
    mov byte [bp-008h], dh                    ; 88 76 f8
    movzx di, dl                              ; 0f b6 fa
    imul di, di, strict byte 00006h           ; 6b ff 06
    add di, si                                ; 01 f7
    mov bx, word [es:di+001c2h]               ; 26 8b 9d c2 01
    mov dx, word [es:di+001c4h]               ; 26 8b 95 c4 01
    mov word [bp-01ch], dx                    ; 89 56 e4
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    mov di, si                                ; 89 f7
    add di, ax                                ; 01 c7
    mov al, byte [es:di+022h]                 ; 26 8a 45 22
    mov byte [bp-006h], al                    ; 88 46 fa
    mov ax, word [es:di+024h]                 ; 26 8b 45 24
    mov word [bp-00ch], ax                    ; 89 46 f4
    test ax, ax                               ; 85 c0
    jne short 01db7h                          ; 75 14
    cmp byte [bp-006h], 001h                  ; 80 7e fa 01
    jne short 01db0h                          ; 75 07
    mov word [bp-00ch], 04000h                ; c7 46 f4 00 40
    jmp short 01dc6h                          ; eb 16
    mov word [bp-00ch], 08000h                ; c7 46 f4 00 80
    jmp short 01dc6h                          ; eb 0f
    cmp byte [bp-006h], 001h                  ; 80 7e fa 01
    jne short 01dc3h                          ; 75 06
    shr word [bp-00ch], 002h                  ; c1 6e f4 02
    jmp short 01dc6h                          ; eb 03
    shr word [bp-00ch], 1                     ; d1 6e f4
    lea dx, [bx+007h]                         ; 8d 57 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 080h                 ; a8 80
    je short 01ddfh                           ; 74 0f
    mov dx, word [bp-01ch]                    ; 8b 56 e4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00001h                ; ba 01 00
    jmp near 01f9eh                           ; e9 bf 01
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov ax, word [es:si]                      ; 26 8b 04
    mov word [bp-020h], ax                    ; 89 46 e0
    mov ax, word [es:si+002h]                 ; 26 8b 44 02
    mov word [bp-01eh], ax                    ; 89 46 e2
    mov di, word [es:si+004h]                 ; 26 8b 7c 04
    mov ax, word [es:si+006h]                 ; 26 8b 44 06
    mov word [bp-00eh], ax                    ; 89 46 f2
    mov ax, word [es:si+012h]                 ; 26 8b 44 12
    mov word [bp-010h], ax                    ; 89 46 f0
    mov ax, word [es:si+00eh]                 ; 26 8b 44 0e
    mov word [bp-012h], ax                    ; 89 46 ee
    mov ax, word [es:si+010h]                 ; 26 8b 44 10
    mov word [bp-018h], ax                    ; 89 46 e8
    mov ax, word [bp-010h]                    ; 8b 46 f0
    test ax, ax                               ; 85 c0
    jne short 01e7dh                          ; 75 67
    mov dx, word [bp-020h]                    ; 8b 56 e0
    add dx, word [bp-014h]                    ; 03 56 ec
    adc ax, word [bp-01eh]                    ; 13 46 e2
    cmp ax, 01000h                            ; 3d 00 10
    jnbe short 01e26h                         ; 77 02
    jne short 01e51h                          ; 75 2b
    mov ax, word [bp-01eh]                    ; 8b 46 e2
    xor al, al                                ; 30 c0
    shr ax, 008h                              ; c1 e8 08
    mov word [bp-010h], ax                    ; 89 46 f0
    mov ax, word [bp-014h]                    ; 8b 46 ec
    xor al, al                                ; 30 c0
    shr ax, 008h                              ; c1 e8 08
    lea dx, [bx+002h]                         ; 8d 57 02
    out DX, AL                                ; ee
    lea dx, [bx+003h]                         ; 8d 57 03
    mov al, byte [bp-010h]                    ; 8a 46 f0
    out DX, AL                                ; ee
    lea dx, [bx+004h]                         ; 8d 57 04
    xor al, al                                ; 30 c0
    out DX, AL                                ; ee
    lea dx, [bx+005h]                         ; 8d 57 05
    out DX, AL                                ; ee
    mov byte [bp-01dh], al                    ; 88 46 e3
    mov ax, word [bp-020h]                    ; 8b 46 e0
    xor ah, ah                                ; 30 e4
    mov word [bp-010h], ax                    ; 89 46 f0
    mov cx, strict word 00008h                ; b9 08 00
    shr word [bp-01eh], 1                     ; d1 6e e2
    rcr word [bp-020h], 1                     ; d1 5e e0
    loop 01e5ch                               ; e2 f8
    mov ax, word [bp-020h]                    ; 8b 46 e0
    mov word [bp-012h], ax                    ; 89 46 ee
    mov ax, word [bp-01eh]                    ; 8b 46 e2
    mov word [bp-020h], ax                    ; 89 46 e0
    mov word [bp-01eh], strict word 00000h    ; c7 46 e2 00 00
    and ax, strict word 0000fh                ; 25 0f 00
    or AL, strict byte 040h                   ; 0c 40
    mov word [bp-018h], ax                    ; 89 46 e8
    mov dx, word [bp-01ch]                    ; 8b 56 e4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 00ah                  ; b0 0a
    out DX, AL                                ; ee
    lea dx, [bx+001h]                         ; 8d 57 01
    xor al, al                                ; 30 c0
    out DX, AL                                ; ee
    lea dx, [bx+002h]                         ; 8d 57 02
    mov al, byte [bp-014h]                    ; 8a 46 ec
    out DX, AL                                ; ee
    lea dx, [bx+003h]                         ; 8d 57 03
    mov al, byte [bp-010h]                    ; 8a 46 f0
    out DX, AL                                ; ee
    mov ax, word [bp-012h]                    ; 8b 46 ee
    lea dx, [bx+004h]                         ; 8d 57 04
    out DX, AL                                ; ee
    shr ax, 008h                              ; c1 e8 08
    lea dx, [bx+005h]                         ; 8d 57 05
    out DX, AL                                ; ee
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00
    je short 01eb3h                           ; 74 05
    mov ax, 000b0h                            ; b8 b0 00
    jmp short 01eb6h                          ; eb 03
    mov ax, 000a0h                            ; b8 a0 00
    movzx dx, byte [bp-018h]                  ; 0f b6 56 e8
    or ax, dx                                 ; 09 d0
    lea dx, [bx+006h]                         ; 8d 57 06
    out DX, AL                                ; ee
    lea dx, [bx+007h]                         ; 8d 57 07
    mov al, byte [bp-016h]                    ; 8a 46 ea
    out DX, AL                                ; ee
    mov ax, word [bp-016h]                    ; 8b 46 ea
    cmp ax, 000c4h                            ; 3d c4 00
    je short 01ed4h                           ; 74 05
    cmp ax, strict word 00029h                ; 3d 29 00
    jne short 01ee1h                          ; 75 0d
    mov ax, word [bp-014h]                    ; 8b 46 ec
    mov word [bp-01ah], ax                    ; 89 46 e6
    mov word [bp-014h], strict word 00001h    ; c7 46 ec 01 00
    jmp short 01ee6h                          ; eb 05
    mov word [bp-01ah], strict word 00001h    ; c7 46 e6 01 00
    lea dx, [bx+007h]                         ; 8d 57 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov dl, al                                ; 88 c2
    test AL, strict byte 080h                 ; a8 80
    jne short 01ee6h                          ; 75 f4
    test AL, strict byte 001h                 ; a8 01
    je short 01f05h                           ; 74 0f
    mov dx, word [bp-01ch]                    ; 8b 56 e4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00002h                ; ba 02 00
    jmp near 01f9eh                           ; e9 99 00
    test dl, 008h                             ; f6 c2 08
    jne short 01f19h                          ; 75 0f
    mov dx, word [bp-01ch]                    ; 8b 56 e4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00003h                ; ba 03 00
    jmp near 01f9eh                           ; e9 85 00
    sti                                       ; fb
    cmp di, 0f800h                            ; 81 ff 00 f8
    jc short 01f2dh                           ; 72 0d
    sub di, 00800h                            ; 81 ef 00 08
    mov ax, word [bp-00eh]                    ; 8b 46 f2
    add ax, 00080h                            ; 05 80 00
    mov word [bp-00eh], ax                    ; 89 46 f2
    cmp byte [bp-006h], 001h                  ; 80 7e fa 01
    jne short 01f40h                          ; 75 0d
    mov dx, bx                                ; 89 da
    mov cx, word [bp-00ch]                    ; 8b 4e f4
    mov es, [bp-00eh]                         ; 8e 46 f2
    db  0f3h, 066h, 06dh
    ; rep insd                                  ; f3 66 6d
    jmp short 01f4ah                          ; eb 0a
    mov dx, bx                                ; 89 da
    mov cx, word [bp-00ch]                    ; 8b 4e f4
    mov es, [bp-00eh]                         ; 8e 46 f2
    rep insw                                  ; f3 6d
    mov ax, word [bp-01ah]                    ; 8b 46 e6
    mov es, [bp-00ah]                         ; 8e 46 f6
    add word [es:si+014h], ax                 ; 26 01 44 14
    dec word [bp-014h]                        ; ff 4e ec
    lea dx, [bx+007h]                         ; 8d 57 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov dl, al                                ; 88 c2
    test AL, strict byte 080h                 ; a8 80
    jne short 01f57h                          ; 75 f4
    cmp word [bp-014h], strict byte 00000h    ; 83 7e ec 00
    jne short 01f7dh                          ; 75 14
    and AL, strict byte 0c9h                  ; 24 c9
    cmp AL, strict byte 040h                  ; 3c 40
    je short 01f93h                           ; 74 24
    mov dx, word [bp-01ch]                    ; 8b 56 e4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00004h                ; ba 04 00
    jmp short 01f9eh                          ; eb 21
    mov al, dl                                ; 88 d0
    and AL, strict byte 0c9h                  ; 24 c9
    cmp AL, strict byte 048h                  ; 3c 48
    je short 01f1ah                           ; 74 95
    mov dx, word [bp-01ch]                    ; 8b 56 e4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00005h                ; ba 05 00
    jmp short 01f9eh                          ; eb 0b
    mov dx, word [bp-01ch]                    ; 8b 56 e4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    xor dx, dx                                ; 31 d2
    mov ax, dx                                ; 89 d0
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn                                      ; c3
_ata_detect:                                 ; 0xf1fa7 LB 0x617
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    push di                                   ; 57
    sub sp, 0025ch                            ; 81 ec 5c 02
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 b3 f6
    mov word [bp-024h], ax                    ; 89 46 dc
    mov di, 00122h                            ; bf 22 01
    mov es, ax                                ; 8e c0
    mov word [bp-028h], di                    ; 89 7e d8
    mov word [bp-026h], ax                    ; 89 46 da
    mov byte [es:di+001c0h], 000h             ; 26 c6 85 c0 01 00
    db  066h, 026h, 0c7h, 085h, 0c2h, 001h, 0f0h, 001h, 0f0h, 003h
    ; mov dword [es:di+001c2h], strict dword 003f001f0h ; 66 26 c7 85 c2 01 f0 01 f0 03
    mov byte [es:di+001c1h], 00eh             ; 26 c6 85 c1 01 0e
    mov byte [es:di+001c6h], 000h             ; 26 c6 85 c6 01 00
    db  066h, 026h, 0c7h, 085h, 0c8h, 001h, 070h, 001h, 070h, 003h
    ; mov dword [es:di+001c8h], strict dword 003700170h ; 66 26 c7 85 c8 01 70 01 70 03
    mov byte [es:di+001c7h], 00fh             ; 26 c6 85 c7 01 0f
    xor al, al                                ; 30 c0
    mov byte [bp-00ah], al                    ; 88 46 f6
    mov byte [bp-006h], al                    ; 88 46 fa
    mov byte [bp-00eh], al                    ; 88 46 f2
    jmp near 02549h                           ; e9 48 05
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
    jne near 020fbh                           ; 0f 85 c0 00
    cmp AL, strict byte 0aah                  ; 3c aa
    jne near 020fbh                           ; 0f 85 ba 00
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    les bx, [bp-028h]                         ; c4 5e d8
    add bx, ax                                ; 01 c3
    mov byte [es:bx+01eh], 001h               ; 26 c6 47 1e 01
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2
    call 01c71h                               ; e8 18 fc
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00
    je short 02064h                           ; 74 05
    mov ax, 000b0h                            ; b8 b0 00
    jmp short 02067h                          ; eb 03
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
    jne near 020fbh                           ; 0f 85 7b 00
    cmp al, bl                                ; 38 d8
    jne near 020fbh                           ; 0f 85 75 00
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
    mov byte [bp-00ch], al                    ; 88 46 f4
    lea dx, [si+007h]                         ; 8d 54 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp bl, 014h                              ; 80 fb 14
    jne short 020beh                          ; 75 18
    cmp cl, 0ebh                              ; 80 f9 eb
    jne short 020beh                          ; 75 13
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    les bx, [bp-028h]                         ; c4 5e d8
    add bx, ax                                ; 01 c3
    mov byte [es:bx+01eh], 003h               ; 26 c6 47 1e 03
    jmp short 020fbh                          ; eb 3d
    test bh, bh                               ; 84 ff
    jne short 020e0h                          ; 75 1e
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00
    jne short 020e0h                          ; 75 18
    test al, al                               ; 84 c0
    je short 020e0h                           ; 74 14
    movzx bx, byte [bp-00eh]                  ; 0f b6 5e f2
    imul bx, bx, strict byte 00018h           ; 6b db 18
    mov es, [bp-026h]                         ; 8e 46 da
    add bx, word [bp-028h]                    ; 03 5e d8
    mov byte [es:bx+01eh], 002h               ; 26 c6 47 1e 02
    jmp short 020fbh                          ; eb 1b
    cmp bh, 0ffh                              ; 80 ff ff
    jne short 020fbh                          ; 75 16
    cmp bh, byte [bp-00ch]                    ; 3a 7e f4
    jne short 020fbh                          ; 75 11
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    les bx, [bp-028h]                         ; c4 5e d8
    add bx, ax                                ; 01 c3
    mov byte [es:bx+01eh], 000h               ; 26 c6 47 1e 00
    mov dx, word [bp-02ch]                    ; 8b 56 d4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    movzx si, byte [bp-00eh]                  ; 0f b6 76 f2
    imul si, si, strict byte 00018h           ; 6b f6 18
    mov es, [bp-026h]                         ; 8e 46 da
    add si, word [bp-028h]                    ; 03 76 d8
    mov al, byte [es:si+01eh]                 ; 26 8a 44 1e
    mov byte [bp-010h], al                    ; 88 46 f0
    cmp AL, strict byte 002h                  ; 3c 02
    jne near 02321h                           ; 0f 85 03 02
    mov byte [es:si+01fh], 0ffh               ; 26 c6 44 1f ff
    mov byte [es:si+022h], 000h               ; 26 c6 44 22 00
    lea dx, [bp-00260h]                       ; 8d 96 a0 fd
    mov bx, word [bp-028h]                    ; 8b 5e d8
    mov word [es:bx+004h], dx                 ; 26 89 57 04
    mov [es:bx+006h], ss                      ; 26 8c 57 06
    mov al, byte [bp-00eh]                    ; 8a 46 f2
    mov byte [es:bx+008h], al                 ; 26 88 47 08
    mov cx, strict word 00001h                ; b9 01 00
    mov bx, 000ech                            ; bb ec 00
    mov ax, word [bp-028h]                    ; 8b 46 d8
    mov dx, es                                ; 8c c2
    call 01d4fh                               ; e8 03 fc
    test ax, ax                               ; 85 c0
    je short 0215bh                           ; 74 0b
    push 00104h                               ; 68 04 01
    push strict byte 00007h                   ; 6a 07
    call 01972h                               ; e8 1a f8
    add sp, strict byte 00004h                ; 83 c4 04
    test byte [bp-00260h], 080h               ; f6 86 a0 fd 80
    db  00fh, 095h, 0c0h
    ; setne al                                  ; 0f 95 c0
    xor ah, ah                                ; 30 e4
    mov byte [bp-014h], al                    ; 88 46 ec
    cmp byte [bp-00200h], 000h                ; 80 be 00 fe 00
    db  00fh, 095h, 0c0h
    ; setne al                                  ; 0f 95 c0
    xor ah, ah                                ; 30 e4
    mov byte [bp-016h], al                    ; 88 46 ea
    mov word [bp-022h], 00200h                ; c7 46 de 00 02
    mov ax, word [bp-0025eh]                  ; 8b 86 a2 fd
    mov word [bp-01ah], ax                    ; 89 46 e6
    mov ax, word [bp-0025ah]                  ; 8b 86 a6 fd
    mov word [bp-01ch], ax                    ; 89 46 e4
    mov ax, word [bp-00254h]                  ; 8b 86 ac fd
    mov word [bp-01eh], ax                    ; 89 46 e2
    mov ax, word [bp-001e8h]                  ; 8b 86 18 fe
    mov word [bp-020h], ax                    ; 89 46 e0
    mov si, word [bp-001e6h]                  ; 8b b6 1a fe
    cmp si, 00fffh                            ; 81 fe ff 0f
    jne short 021b0h                          ; 75 10
    cmp ax, strict word 0ffffh                ; 3d ff ff
    jne short 021b0h                          ; 75 0b
    mov ax, word [bp-00198h]                  ; 8b 86 68 fe
    mov word [bp-020h], ax                    ; 89 46 e0
    mov si, word [bp-00196h]                  ; 8b b6 6a fe
    mov al, byte [bp-00eh]                    ; 8a 46 f2
    cmp AL, strict byte 001h                  ; 3c 01
    jc short 021c3h                           ; 72 0c
    jbe short 021cbh                          ; 76 12
    cmp AL, strict byte 003h                  ; 3c 03
    je short 021d3h                           ; 74 16
    cmp AL, strict byte 002h                  ; 3c 02
    je short 021cfh                           ; 74 0e
    jmp short 02210h                          ; eb 4d
    test al, al                               ; 84 c0
    jne short 02210h                          ; 75 49
    mov BL, strict byte 01eh                  ; b3 1e
    jmp short 021d5h                          ; eb 0a
    mov BL, strict byte 026h                  ; b3 26
    jmp short 021d5h                          ; eb 06
    mov BL, strict byte 067h                  ; b3 67
    jmp short 021d5h                          ; eb 02
    mov BL, strict byte 070h                  ; b3 70
    mov al, bl                                ; 88 d8
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    xor ah, ah                                ; 30 e4
    call 016ach                               ; e8 ce f4
    xor ah, ah                                ; 30 e4
    mov dx, ax                                ; 89 c2
    sal dx, 008h                              ; c1 e2 08
    movzx ax, bl                              ; 0f b6 c3
    call 016ach                               ; e8 c1 f4
    xor ah, ah                                ; 30 e4
    add ax, dx                                ; 01 d0
    mov word [bp-034h], ax                    ; 89 46 cc
    mov al, bl                                ; 88 d8
    add AL, strict byte 002h                  ; 04 02
    xor ah, ah                                ; 30 e4
    call 016ach                               ; e8 b1 f4
    xor ah, ah                                ; 30 e4
    mov word [bp-036h], ax                    ; 89 46 ca
    mov al, bl                                ; 88 d8
    add AL, strict byte 007h                  ; 04 07
    xor ah, ah                                ; 30 e4
    call 016ach                               ; e8 a3 f4
    xor ah, ah                                ; 30 e4
    mov word [bp-032h], ax                    ; 89 46 ce
    jmp short 0221dh                          ; eb 0d
    mov bx, word [bp-020h]                    ; 8b 5e e0
    mov cx, si                                ; 89 f1
    mov dx, ss                                ; 8c d2
    lea ax, [bp-036h]                         ; 8d 46 ca
    call 05389h                               ; e8 6c 31
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 09 f7
    mov ax, word [bp-032h]                    ; 8b 46 ce
    push ax                                   ; 50
    mov ax, word [bp-036h]                    ; 8b 46 ca
    push ax                                   ; 50
    mov ax, word [bp-034h]                    ; 8b 46 cc
    push ax                                   ; 50
    push word [bp-01eh]                       ; ff 76 e2
    push word [bp-01ch]                       ; ff 76 e4
    push word [bp-01ah]                       ; ff 76 e6
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    push ax                                   ; 50
    movzx ax, byte [bp-018h]                  ; 0f b6 46 e8
    push ax                                   ; 50
    push 0012dh                               ; 68 2d 01
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 23 f7
    add sp, strict byte 00014h                ; 83 c4 14
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    les di, [bp-028h]                         ; c4 7e d8
    add di, ax                                ; 01 c7
    mov byte [es:di+01fh], 0ffh               ; 26 c6 45 1f ff
    mov al, byte [bp-014h]                    ; 8a 46 ec
    mov byte [es:di+020h], al                 ; 26 88 45 20
    mov al, byte [bp-016h]                    ; 8a 46 ea
    mov byte [es:di+022h], al                 ; 26 88 45 22
    mov ax, word [bp-022h]                    ; 8b 46 de
    mov word [es:di+024h], ax                 ; 26 89 45 24
    mov ax, word [bp-01ch]                    ; 8b 46 e4
    mov word [es:di+02ch], ax                 ; 26 89 45 2c
    mov ax, word [bp-01ah]                    ; 8b 46 e6
    mov word [es:di+02eh], ax                 ; 26 89 45 2e
    mov ax, word [bp-01eh]                    ; 8b 46 e2
    mov word [es:di+030h], ax                 ; 26 89 45 30
    mov ax, word [bp-020h]                    ; 8b 46 e0
    mov word [es:di+032h], ax                 ; 26 89 45 32
    mov word [es:di+034h], si                 ; 26 89 75 34
    lea di, [di+026h]                         ; 8d 7d 26
    push DS                                   ; 1e
    push SS                                   ; 16
    pop DS                                    ; 1f
    lea si, [bp-036h]                         ; 8d 76 ca
    movsw                                     ; a5
    movsw                                     ; a5
    movsw                                     ; a5
    pop DS                                    ; 1f
    mov al, byte [bp-00eh]                    ; 8a 46 f2
    cmp AL, strict byte 002h                  ; 3c 02
    jnc short 0230ch                          ; 73 60
    test al, al                               ; 84 c0
    jne short 022b5h                          ; 75 05
    mov di, strict word 0003dh                ; bf 3d 00
    jmp short 022b8h                          ; eb 03
    mov di, strict word 0004dh                ; bf 4d 00
    mov dx, word [bp-024h]                    ; 8b 56 dc
    mov ax, word [bp-034h]                    ; 8b 46 cc
    mov es, dx                                ; 8e c2
    mov word [es:di], ax                      ; 26 89 05
    mov al, byte [bp-036h]                    ; 8a 46 ca
    mov byte [es:di+002h], al                 ; 26 88 45 02
    mov byte [es:di+003h], 0a0h               ; 26 c6 45 03 a0
    mov al, byte [bp-01eh]                    ; 8a 46 e2
    mov byte [es:di+004h], al                 ; 26 88 45 04
    mov ax, word [bp-01ah]                    ; 8b 46 e6
    mov word [es:di+009h], ax                 ; 26 89 45 09
    mov al, byte [bp-01ch]                    ; 8a 46 e4
    mov byte [es:di+00bh], al                 ; 26 88 45 0b
    mov al, byte [bp-01eh]                    ; 8a 46 e2
    mov byte [es:di+00eh], al                 ; 26 88 45 0e
    xor al, al                                ; 30 c0
    xor ah, ah                                ; 30 e4
    jmp short 022f6h                          ; eb 05
    cmp ah, 00fh                              ; 80 fc 0f
    jnc short 02304h                          ; 73 0e
    movzx bx, ah                              ; 0f b6 dc
    mov es, dx                                ; 8e c2
    add bx, di                                ; 01 fb
    add al, byte [es:bx]                      ; 26 02 07
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4
    jmp short 022f1h                          ; eb ed
    neg al                                    ; f6 d8
    mov es, dx                                ; 8e c2
    mov byte [es:di+00fh], al                 ; 26 88 45 0f
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa
    mov es, [bp-026h]                         ; 8e 46 da
    add bx, word [bp-028h]                    ; 03 5e d8
    mov al, byte [bp-00eh]                    ; 8a 46 f2
    mov byte [es:bx+0019fh], al               ; 26 88 87 9f 01
    inc byte [bp-006h]                        ; fe 46 fa
    cmp byte [bp-010h], 003h                  ; 80 7e f0 03
    jne near 023c1h                           ; 0f 85 98 00
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    les bx, [bp-028h]                         ; c4 5e d8
    add bx, ax                                ; 01 c3
    mov byte [es:bx+01fh], 005h               ; 26 c6 47 1f 05
    mov byte [es:bx+022h], 000h               ; 26 c6 47 22 00
    lea dx, [bp-00260h]                       ; 8d 96 a0 fd
    mov bx, word [bp-028h]                    ; 8b 5e d8
    mov word [es:bx+004h], dx                 ; 26 89 57 04
    mov [es:bx+006h], ss                      ; 26 8c 57 06
    mov al, byte [bp-00eh]                    ; 8a 46 f2
    mov byte [es:bx+008h], al                 ; 26 88 47 08
    mov cx, strict word 00001h                ; b9 01 00
    mov bx, 000a1h                            ; bb a1 00
    mov ax, word [bp-028h]                    ; 8b 46 d8
    mov dx, es                                ; 8c c2
    call 01d4fh                               ; e8 ec f9
    test ax, ax                               ; 85 c0
    je short 02372h                           ; 74 0b
    push 00154h                               ; 68 54 01
    push strict byte 00007h                   ; 6a 07
    call 01972h                               ; e8 03 f6
    add sp, strict byte 00004h                ; 83 c4 04
    mov cl, byte [bp-0025fh]                  ; 8a 8e a1 fd
    and cl, 01fh                              ; 80 e1 1f
    test byte [bp-00260h], 080h               ; f6 86 a0 fd 80
    db  00fh, 095h, 0c0h
    ; setne al                                  ; 0f 95 c0
    xor ah, ah                                ; 30 e4
    mov dx, ax                                ; 89 c2
    cmp byte [bp-00200h], 000h                ; 80 be 00 fe 00
    db  00fh, 095h, 0c0h
    ; setne al                                  ; 0f 95 c0
    xor ah, ah                                ; 30 e4
    mov bx, ax                                ; 89 c3
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    les si, [bp-028h]                         ; c4 76 d8
    add si, ax                                ; 01 c6
    mov byte [es:si+01fh], cl                 ; 26 88 4c 1f
    mov byte [es:si+020h], dl                 ; 26 88 54 20
    mov byte [es:si+022h], bl                 ; 26 88 5c 22
    mov word [es:si+024h], 00800h             ; 26 c7 44 24 00 08
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6
    add bx, word [bp-028h]                    ; 03 5e d8
    mov al, byte [bp-00eh]                    ; 8a 46 f2
    mov byte [es:bx+001b0h], al               ; 26 88 87 b0 01
    inc byte [bp-00ah]                        ; fe 46 f6
    mov al, byte [bp-010h]                    ; 8a 46 f0
    cmp AL, strict byte 003h                  ; 3c 03
    je short 023f4h                           ; 74 2c
    cmp AL, strict byte 002h                  ; 3c 02
    jne near 02457h                           ; 0f 85 89 00
    movzx bx, byte [bp-00eh]                  ; 0f b6 5e f2
    imul bx, bx, strict byte 00018h           ; 6b db 18
    mov es, [bp-026h]                         ; 8e 46 da
    add bx, word [bp-028h]                    ; 03 5e d8
    mov ax, word [es:bx+032h]                 ; 26 8b 47 32
    mov word [bp-030h], ax                    ; 89 46 d0
    mov ax, word [es:bx+034h]                 ; 26 8b 47 34
    mov word [bp-02eh], ax                    ; 89 46 d2
    mov cx, strict word 0000bh                ; b9 0b 00
    shr word [bp-02eh], 1                     ; d1 6e d2
    rcr word [bp-030h], 1                     ; d1 5e d0
    loop 023ech                               ; e2 f8
    movzx dx, byte [bp-001bfh]                ; 0f b6 96 41 fe
    sal dx, 008h                              ; c1 e2 08
    movzx ax, byte [bp-001c0h]                ; 0f b6 86 40 fe
    or dx, ax                                 ; 09 c2
    mov byte [bp-012h], 00fh                  ; c6 46 ee 0f
    jmp short 02412h                          ; eb 09
    dec byte [bp-012h]                        ; fe 4e ee
    cmp byte [bp-012h], 000h                  ; 80 7e ee 00
    jbe short 0241fh                          ; 76 0d
    movzx cx, byte [bp-012h]                  ; 0f b6 4e ee
    mov ax, strict word 00001h                ; b8 01 00
    sal ax, CL                                ; d3 e0
    test dx, ax                               ; 85 c2
    je short 02409h                           ; 74 ea
    xor di, di                                ; 31 ff
    jmp short 02428h                          ; eb 05
    cmp di, strict byte 00014h                ; 83 ff 14
    jnl short 0243dh                          ; 7d 15
    mov si, di                                ; 89 fe
    add si, di                                ; 01 fe
    mov al, byte [bp+si-00229h]               ; 8a 82 d7 fd
    mov byte [bp+si-060h], al                 ; 88 42 a0
    mov al, byte [bp+si-0022ah]               ; 8a 82 d6 fd
    mov byte [bp+si-05fh], al                 ; 88 42 a1
    inc di                                    ; 47
    jmp short 02423h                          ; eb e6
    mov byte [bp-038h], 000h                  ; c6 46 c8 00
    mov di, strict word 00027h                ; bf 27 00
    jmp short 0244bh                          ; eb 05
    dec di                                    ; 4f
    test di, di                               ; 85 ff
    jle short 02457h                          ; 7e 0c
    cmp byte [bp+di-060h], 020h               ; 80 7b a0 20
    jne short 02457h                          ; 75 06
    mov byte [bp+di-060h], 000h               ; c6 43 a0 00
    jmp short 02446h                          ; eb ef
    mov al, byte [bp-010h]                    ; 8a 46 f0
    cmp AL, strict byte 003h                  ; 3c 03
    je short 024bah                           ; 74 5c
    cmp AL, strict byte 002h                  ; 3c 02
    je short 0246bh                           ; 74 09
    cmp AL, strict byte 001h                  ; 3c 01
    je near 02521h                            ; 0f 84 b9 00
    jmp near 02540h                           ; e9 d5 00
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00
    je short 02476h                           ; 74 05
    mov ax, 0017fh                            ; b8 7f 01
    jmp short 02479h                          ; eb 03
    mov ax, 00186h                            ; b8 86 01
    push ax                                   ; 50
    movzx ax, byte [bp-018h]                  ; 0f b6 46 e8
    push ax                                   ; 50
    push 0018dh                               ; 68 8d 01
    push strict byte 00002h                   ; 6a 02
    call 01972h                               ; e8 eb f4
    add sp, strict byte 00008h                ; 83 c4 08
    xor di, di                                ; 31 ff
    movzx ax, byte [bp+di-060h]               ; 0f b6 43 a0
    inc di                                    ; 47
    test ax, ax                               ; 85 c0
    je short 024a3h                           ; 74 0e
    push ax                                   ; 50
    push 00198h                               ; 68 98 01
    push strict byte 00002h                   ; 6a 02
    call 01972h                               ; e8 d4 f4
    add sp, strict byte 00006h                ; 83 c4 06
    jmp short 0248ch                          ; eb e9
    push dword [bp-030h]                      ; 66 ff 76 d0
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee
    push ax                                   ; 50
    push 0019bh                               ; 68 9b 01
    push strict byte 00002h                   ; 6a 02
    call 01972h                               ; e8 be f4
    add sp, strict byte 0000ah                ; 83 c4 0a
    jmp near 02540h                           ; e9 86 00
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00
    je short 024c5h                           ; 74 05
    mov ax, 0017fh                            ; b8 7f 01
    jmp short 024c8h                          ; eb 03
    mov ax, 00186h                            ; b8 86 01
    push ax                                   ; 50
    movzx ax, byte [bp-018h]                  ; 0f b6 46 e8
    push ax                                   ; 50
    push 0018dh                               ; 68 8d 01
    push strict byte 00002h                   ; 6a 02
    call 01972h                               ; e8 9c f4
    add sp, strict byte 00008h                ; 83 c4 08
    xor di, di                                ; 31 ff
    movzx ax, byte [bp+di-060h]               ; 0f b6 43 a0
    inc di                                    ; 47
    test ax, ax                               ; 85 c0
    je short 024f2h                           ; 74 0e
    push ax                                   ; 50
    push 00198h                               ; 68 98 01
    push strict byte 00002h                   ; 6a 02
    call 01972h                               ; e8 85 f4
    add sp, strict byte 00006h                ; 83 c4 06
    jmp short 024dbh                          ; eb e9
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    les bx, [bp-028h]                         ; c4 5e d8
    add bx, ax                                ; 01 c3
    cmp byte [es:bx+01fh], 005h               ; 26 80 7f 1f 05
    jne short 0250fh                          ; 75 0a
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee
    push ax                                   ; 50
    push 001bbh                               ; 68 bb 01
    jmp short 02517h                          ; eb 08
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee
    push ax                                   ; 50
    push 001d5h                               ; 68 d5 01
    push strict byte 00002h                   ; 6a 02
    call 01972h                               ; e8 56 f4
    add sp, strict byte 00006h                ; 83 c4 06
    jmp short 02540h                          ; eb 1f
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00
    je short 0252ch                           ; 74 05
    mov ax, 0017fh                            ; b8 7f 01
    jmp short 0252fh                          ; eb 03
    mov ax, 00186h                            ; b8 86 01
    push ax                                   ; 50
    movzx ax, byte [bp-018h]                  ; 0f b6 46 e8
    push ax                                   ; 50
    push 001e7h                               ; 68 e7 01
    push strict byte 00002h                   ; 6a 02
    call 01972h                               ; e8 35 f4
    add sp, strict byte 00008h                ; 83 c4 08
    inc byte [bp-00eh]                        ; fe 46 f2
    cmp byte [bp-00eh], 008h                  ; 80 7e f2 08
    jnc short 02597h                          ; 73 4e
    movzx bx, byte [bp-00eh]                  ; 0f b6 5e f2
    mov ax, bx                                ; 89 d8
    cwd                                       ; 99
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2
    sar ax, 1                                 ; d1 f8
    mov word [bp-02ah], ax                    ; 89 46 d6
    mov al, byte [bp-02ah]                    ; 8a 46 d6
    mov byte [bp-018h], al                    ; 88 46 e8
    mov ax, bx                                ; 89 d8
    cwd                                       ; 99
    mov bx, strict word 00002h                ; bb 02 00
    idiv bx                                   ; f7 fb
    mov cx, dx                                ; 89 d1
    mov byte [bp-008h], dl                    ; 88 56 f8
    movzx ax, byte [bp-02ah]                  ; 0f b6 46 d6
    imul ax, ax, strict byte 00006h           ; 6b c0 06
    les bx, [bp-028h]                         ; c4 5e d8
    add bx, ax                                ; 01 c3
    mov si, word [es:bx+001c2h]               ; 26 8b b7 c2 01
    mov ax, word [es:bx+001c4h]               ; 26 8b 87 c4 01
    mov word [bp-02ch], ax                    ; 89 46 d4
    mov dx, ax                                ; 89 c2
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 00ah                  ; b0 0a
    out DX, AL                                ; ee
    test cl, cl                               ; 84 c9
    je near 02001h                            ; 0f 84 70 fa
    mov ax, 000b0h                            ; b8 b0 00
    jmp near 02004h                           ; e9 6d fa
    mov al, byte [bp-006h]                    ; 8a 46 fa
    les bx, [bp-028h]                         ; c4 5e d8
    mov byte [es:bx+0019eh], al               ; 26 88 87 9e 01
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    mov byte [es:bx+001afh], al               ; 26 88 87 af 01
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa
    mov dx, strict word 00075h                ; ba 75 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 a7 f0
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn                                      ; c3
ata_cmd_data_out_:                           ; 0xf25be LB 0x21a
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    push di                                   ; 57
    sub sp, strict byte 0001ah                ; 83 ec 1a
    mov di, ax                                ; 89 c7
    mov word [bp-00ah], dx                    ; 89 56 f6
    mov word [bp-01ah], bx                    ; 89 5e e6
    mov word [bp-00eh], cx                    ; 89 4e f2
    mov es, dx                                ; 8e c2
    movzx ax, byte [es:di+008h]               ; 26 0f b6 45 08
    mov dx, ax                                ; 89 c2
    shr dx, 1                                 ; d1 ea
    mov dh, al                                ; 88 c6
    and dh, 001h                              ; 80 e6 01
    mov byte [bp-006h], dh                    ; 88 76 fa
    movzx si, dl                              ; 0f b6 f2
    imul si, si, strict byte 00006h           ; 6b f6 06
    add si, di                                ; 01 fe
    mov bx, word [es:si+001c2h]               ; 26 8b 9c c2 01
    mov dx, word [es:si+001c4h]               ; 26 8b 94 c4 01
    mov word [bp-00ch], dx                    ; 89 56 f4
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    mov si, di                                ; 89 fe
    add si, ax                                ; 01 c6
    mov al, byte [es:si+022h]                 ; 26 8a 44 22
    mov byte [bp-008h], al                    ; 88 46 f8
    cmp AL, strict byte 001h                  ; 3c 01
    jne short 02612h                          ; 75 07
    mov word [bp-012h], 00080h                ; c7 46 ee 80 00
    jmp short 02617h                          ; eb 05
    mov word [bp-012h], 00100h                ; c7 46 ee 00 01
    lea dx, [bx+007h]                         ; 8d 57 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 080h                 ; a8 80
    je short 02630h                           ; 74 0f
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00001h                ; ba 01 00
    jmp near 027cfh                           ; e9 9f 01
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov ax, word [es:di]                      ; 26 8b 05
    mov word [bp-01eh], ax                    ; 89 46 e2
    mov ax, word [es:di+002h]                 ; 26 8b 45 02
    mov word [bp-01ch], ax                    ; 89 46 e4
    mov si, word [es:di+004h]                 ; 26 8b 75 04
    mov ax, word [es:di+006h]                 ; 26 8b 45 06
    mov word [bp-018h], ax                    ; 89 46 e8
    mov ax, word [es:di+012h]                 ; 26 8b 45 12
    mov word [bp-010h], ax                    ; 89 46 f0
    mov ax, word [es:di+00eh]                 ; 26 8b 45 0e
    mov word [bp-016h], ax                    ; 89 46 ea
    mov ax, word [es:di+010h]                 ; 26 8b 45 10
    mov word [bp-014h], ax                    ; 89 46 ec
    mov ax, word [bp-010h]                    ; 8b 46 f0
    test ax, ax                               ; 85 c0
    jne short 026ceh                          ; 75 67
    mov dx, word [bp-01eh]                    ; 8b 56 e2
    add dx, word [bp-00eh]                    ; 03 56 f2
    adc ax, word [bp-01ch]                    ; 13 46 e4
    cmp ax, 01000h                            ; 3d 00 10
    jnbe short 02677h                         ; 77 02
    jne short 026a2h                          ; 75 2b
    mov ax, word [bp-01ch]                    ; 8b 46 e4
    xor al, al                                ; 30 c0
    shr ax, 008h                              ; c1 e8 08
    mov word [bp-010h], ax                    ; 89 46 f0
    mov ax, word [bp-00eh]                    ; 8b 46 f2
    xor al, al                                ; 30 c0
    shr ax, 008h                              ; c1 e8 08
    lea dx, [bx+002h]                         ; 8d 57 02
    out DX, AL                                ; ee
    lea dx, [bx+003h]                         ; 8d 57 03
    mov al, byte [bp-010h]                    ; 8a 46 f0
    out DX, AL                                ; ee
    lea dx, [bx+004h]                         ; 8d 57 04
    xor al, al                                ; 30 c0
    out DX, AL                                ; ee
    lea dx, [bx+005h]                         ; 8d 57 05
    out DX, AL                                ; ee
    mov byte [bp-01bh], al                    ; 88 46 e5
    mov ax, word [bp-01eh]                    ; 8b 46 e2
    xor ah, ah                                ; 30 e4
    mov word [bp-010h], ax                    ; 89 46 f0
    mov cx, strict word 00008h                ; b9 08 00
    shr word [bp-01ch], 1                     ; d1 6e e4
    rcr word [bp-01eh], 1                     ; d1 5e e2
    loop 026adh                               ; e2 f8
    mov ax, word [bp-01eh]                    ; 8b 46 e2
    mov word [bp-016h], ax                    ; 89 46 ea
    mov ax, word [bp-01ch]                    ; 8b 46 e4
    mov word [bp-01eh], ax                    ; 89 46 e2
    mov word [bp-01ch], strict word 00000h    ; c7 46 e4 00 00
    and ax, strict word 0000fh                ; 25 0f 00
    or AL, strict byte 040h                   ; 0c 40
    mov word [bp-014h], ax                    ; 89 46 ec
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 00ah                  ; b0 0a
    out DX, AL                                ; ee
    lea dx, [bx+001h]                         ; 8d 57 01
    xor al, al                                ; 30 c0
    out DX, AL                                ; ee
    lea dx, [bx+002h]                         ; 8d 57 02
    mov al, byte [bp-00eh]                    ; 8a 46 f2
    out DX, AL                                ; ee
    lea dx, [bx+003h]                         ; 8d 57 03
    mov al, byte [bp-010h]                    ; 8a 46 f0
    out DX, AL                                ; ee
    mov ax, word [bp-016h]                    ; 8b 46 ea
    lea dx, [bx+004h]                         ; 8d 57 04
    out DX, AL                                ; ee
    shr ax, 008h                              ; c1 e8 08
    lea dx, [bx+005h]                         ; 8d 57 05
    out DX, AL                                ; ee
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00
    je short 02704h                           ; 74 05
    mov ax, 000b0h                            ; b8 b0 00
    jmp short 02707h                          ; eb 03
    mov ax, 000a0h                            ; b8 a0 00
    movzx dx, byte [bp-014h]                  ; 0f b6 56 ec
    or ax, dx                                 ; 09 d0
    lea dx, [bx+006h]                         ; 8d 57 06
    out DX, AL                                ; ee
    lea dx, [bx+007h]                         ; 8d 57 07
    mov al, byte [bp-01ah]                    ; 8a 46 e6
    out DX, AL                                ; ee
    lea dx, [bx+007h]                         ; 8d 57 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov dl, al                                ; 88 c2
    test AL, strict byte 080h                 ; a8 80
    jne short 02718h                          ; 75 f4
    test AL, strict byte 001h                 ; a8 01
    je short 02737h                           ; 74 0f
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00002h                ; ba 02 00
    jmp near 027cfh                           ; e9 98 00
    test dl, 008h                             ; f6 c2 08
    jne short 0274bh                          ; 75 0f
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00003h                ; ba 03 00
    jmp near 027cfh                           ; e9 84 00
    sti                                       ; fb
    cmp si, 0f800h                            ; 81 fe 00 f8
    jc short 0275fh                           ; 72 0d
    sub si, 00800h                            ; 81 ee 00 08
    mov ax, word [bp-018h]                    ; 8b 46 e8
    add ax, 00080h                            ; 05 80 00
    mov word [bp-018h], ax                    ; 89 46 e8
    cmp byte [bp-008h], 001h                  ; 80 7e f8 01
    jne short 02773h                          ; 75 0e
    mov dx, bx                                ; 89 da
    mov cx, word [bp-012h]                    ; 8b 4e ee
    mov es, [bp-018h]                         ; 8e 46 e8
    db  0f3h, 066h, 026h, 06fh
    ; rep es outsd                              ; f3 66 26 6f
    jmp short 0277eh                          ; eb 0b
    mov dx, bx                                ; 89 da
    mov cx, word [bp-012h]                    ; 8b 4e ee
    mov es, [bp-018h]                         ; 8e 46 e8
    db  0f3h, 026h, 06fh
    ; rep es outsw                              ; f3 26 6f
    mov es, [bp-00ah]                         ; 8e 46 f6
    inc word [es:di+014h]                     ; 26 ff 45 14
    dec word [bp-00eh]                        ; ff 4e f2
    lea dx, [bx+007h]                         ; 8d 57 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov dl, al                                ; 88 c2
    test AL, strict byte 080h                 ; a8 80
    jne short 02788h                          ; 75 f4
    cmp word [bp-00eh], strict byte 00000h    ; 83 7e f2 00
    jne short 027aeh                          ; 75 14
    and AL, strict byte 0e9h                  ; 24 e9
    cmp AL, strict byte 040h                  ; 3c 40
    je short 027c4h                           ; 74 24
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00006h                ; ba 06 00
    jmp short 027cfh                          ; eb 21
    mov al, dl                                ; 88 d0
    and AL, strict byte 0c9h                  ; 24 c9
    cmp AL, strict byte 048h                  ; 3c 48
    je short 0274ch                           ; 74 96
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00007h                ; ba 07 00
    jmp short 027cfh                          ; eb 0b
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    xor dx, dx                                ; 31 d2
    mov ax, dx                                ; 89 d0
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn                                      ; c3
@ata_read_sectors:                           ; 0xf27d8 LB 0x7a
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    push di                                   ; 57
    push ax                                   ; 50
    mov si, word [bp+004h]                    ; 8b 76 04
    mov es, [bp+006h]                         ; 8e 46 06
    mov bl, byte [es:si+008h]                 ; 26 8a 5c 08
    mov cx, word [es:si+00ah]                 ; 26 8b 4c 0a
    mov dx, cx                                ; 89 ca
    sal dx, 009h                              ; c1 e2 09
    mov ax, word [es:si+012h]                 ; 26 8b 44 12
    test ax, ax                               ; 85 c0
    je short 02806h                           ; 74 0d
    movzx di, bl                              ; 0f b6 fb
    imul di, di, strict byte 00018h           ; 6b ff 18
    mov [bp-006h], es                         ; 8c 46 fa
    add di, si                                ; 01 f7
    jmp short 02832h                          ; eb 2c
    mov di, word [es:si]                      ; 26 8b 3c
    add di, cx                                ; 01 cf
    mov word [bp-006h], di                    ; 89 7e fa
    adc ax, word [es:si+002h]                 ; 26 13 44 02
    cmp ax, 01000h                            ; 3d 00 10
    jnbe short 02819h                         ; 77 02
    jne short 02825h                          ; 75 0c
    mov bx, strict word 00024h                ; bb 24 00
    mov ax, si                                ; 89 f0
    mov dx, es                                ; 8c c2
    call 01d4fh                               ; e8 2c f5
    jmp short 02849h                          ; eb 24
    movzx ax, bl                              ; 0f b6 c3
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    mov [bp-006h], es                         ; 8c 46 fa
    mov di, si                                ; 89 f7
    add di, ax                                ; 01 c7
    mov word [es:di+024h], dx                 ; 26 89 55 24
    mov bx, 000c4h                            ; bb c4 00
    mov ax, si                                ; 89 f0
    mov dx, es                                ; 8c c2
    call 01d4fh                               ; e8 0f f5
    mov es, [bp-006h]                         ; 8e 46 fa
    mov word [es:di+024h], 00200h             ; 26 c7 45 24 00 02
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn 00004h                               ; c2 04 00
@ata_write_sectors:                          ; 0xf2852 LB 0x3d
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    les si, [bp+004h]                         ; c4 76 04
    mov cx, word [es:si+00ah]                 ; 26 8b 4c 0a
    cmp word [es:si+012h], strict byte 00000h ; 26 83 7c 12 00
    je short 02870h                           ; 74 0c
    mov bx, strict word 00030h                ; bb 30 00
    mov ax, si                                ; 89 f0
    mov dx, es                                ; 8c c2
    call 025beh                               ; e8 50 fd
    jmp short 02887h                          ; eb 17
    xor ax, ax                                ; 31 c0
    mov dx, word [es:si]                      ; 26 8b 14
    add dx, cx                                ; 01 ca
    adc ax, word [es:si+002h]                 ; 26 13 44 02
    cmp ax, 01000h                            ; 3d 00 10
    jnbe short 02882h                         ; 77 02
    jne short 02864h                          ; 75 e2
    mov bx, strict word 00034h                ; bb 34 00
    jmp short 02867h                          ; eb e0
    lea sp, [bp-002h]                         ; 8d 66 fe
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn 00004h                               ; c2 04 00
ata_cmd_packet_:                             ; 0xf288f LB 0x2e8
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    push di                                   ; 57
    sub sp, strict byte 00014h                ; 83 ec 14
    push ax                                   ; 50
    mov byte [bp-008h], dl                    ; 88 56 f8
    mov di, bx                                ; 89 df
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 c6 ed
    mov word [bp-012h], 00122h                ; c7 46 ee 22 01
    mov word [bp-00eh], ax                    ; 89 46 f2
    mov ax, word [bp-01ah]                    ; 8b 46 e6
    shr ax, 1                                 ; d1 e8
    mov ah, byte [bp-01ah]                    ; 8a 66 e6
    and ah, 001h                              ; 80 e4 01
    mov byte [bp-006h], ah                    ; 88 66 fa
    cmp byte [bp+00ah], 002h                  ; 80 7e 0a 02
    jne short 028e1h                          ; 75 1f
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 64 f0
    push 00201h                               ; 68 01 02
    push 00210h                               ; 68 10 02
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 9a f0
    add sp, strict byte 00006h                ; 83 c4 06
    mov dx, strict word 00001h                ; ba 01 00
    jmp near 02b6ch                           ; e9 8b 02
    test byte [bp+004h], 001h                 ; f6 46 04 01
    jne short 028dbh                          ; 75 f4
    xor ah, ah                                ; 30 e4
    imul ax, ax, strict byte 00006h           ; 6b c0 06
    mov es, [bp-00eh]                         ; 8e 46 f2
    mov si, word [bp-012h]                    ; 8b 76 ee
    add si, ax                                ; 01 c6
    mov bx, word [es:si+001c2h]               ; 26 8b 9c c2 01
    mov ax, word [es:si+001c4h]               ; 26 8b 84 c4 01
    mov word [bp-010h], ax                    ; 89 46 f0
    imul si, word [bp-01ah], strict byte 00018h ; 6b 76 e6 18
    add si, word [bp-012h]                    ; 03 76 ee
    mov al, byte [es:si+022h]                 ; 26 8a 44 22
    mov byte [bp-00ah], al                    ; 88 46 f6
    xor ax, ax                                ; 31 c0
    mov word [bp-018h], ax                    ; 89 46 e8
    mov word [bp-016h], ax                    ; 89 46 ea
    mov al, byte [bp-008h]                    ; 8a 46 f8
    cmp AL, strict byte 00ch                  ; 3c 0c
    jnc short 02924h                          ; 73 06
    mov byte [bp-008h], 00ch                  ; c6 46 f8 0c
    jmp short 0292ah                          ; eb 06
    jbe short 0292ah                          ; 76 04
    mov byte [bp-008h], 010h                  ; c6 46 f8 10
    shr byte [bp-008h], 1                     ; d0 6e f8
    mov es, [bp-00eh]                         ; 8e 46 f2
    mov si, word [bp-012h]                    ; 8b 76 ee
    db  066h, 026h, 0c7h, 044h, 014h, 000h, 000h, 000h, 000h
    ; mov dword [es:si+014h], strict dword 000000000h ; 66 26 c7 44 14 00 00 00 00
    mov word [es:si+018h], strict word 00000h ; 26 c7 44 18 00 00
    lea dx, [bx+007h]                         ; 8d 57 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 080h                 ; a8 80
    je short 02952h                           ; 74 06
    mov dx, strict word 00002h                ; ba 02 00
    jmp near 02b6ch                           ; e9 1a 02
    mov dx, word [bp-010h]                    ; 8b 56 f0
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 00ah                  ; b0 0a
    out DX, AL                                ; ee
    lea dx, [bx+004h]                         ; 8d 57 04
    mov AL, strict byte 0f0h                  ; b0 f0
    out DX, AL                                ; ee
    lea dx, [bx+005h]                         ; 8d 57 05
    mov AL, strict byte 0ffh                  ; b0 ff
    out DX, AL                                ; ee
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00
    je short 02972h                           ; 74 05
    mov ax, 000b0h                            ; b8 b0 00
    jmp short 02975h                          ; eb 03
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
    jne short 0297fh                          ; 75 f4
    test AL, strict byte 001h                 ; a8 01
    je short 0299eh                           ; 74 0f
    mov dx, word [bp-010h]                    ; 8b 56 f0
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00003h                ; ba 03 00
    jmp near 02b6ch                           ; e9 ce 01
    test dl, 008h                             ; f6 c2 08
    jne short 029b2h                          ; 75 0f
    mov dx, word [bp-010h]                    ; 8b 56 f0
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00004h                ; ba 04 00
    jmp near 02b6ch                           ; e9 ba 01
    sti                                       ; fb
    mov ax, di                                ; 89 f8
    shr ax, 004h                              ; c1 e8 04
    add ax, cx                                ; 01 c8
    mov si, di                                ; 89 fe
    and si, strict byte 0000fh                ; 83 e6 0f
    movzx cx, byte [bp-008h]                  ; 0f b6 4e f8
    mov dx, bx                                ; 89 da
    mov es, ax                                ; 8e c0
    db  0f3h, 026h, 06fh
    ; rep es outsw                              ; f3 26 6f
    cmp byte [bp+00ah], 000h                  ; 80 7e 0a 00
    jne short 029dbh                          ; 75 0b
    lea dx, [bx+007h]                         ; 8d 57 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov dl, al                                ; 88 c2
    jmp near 02b4dh                           ; e9 72 01
    lea dx, [bx+007h]                         ; 8d 57 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov dl, al                                ; 88 c2
    test AL, strict byte 080h                 ; a8 80
    jne short 029dbh                          ; 75 f4
    test AL, strict byte 088h                 ; a8 88
    je near 02b4dh                            ; 0f 84 60 01
    test AL, strict byte 001h                 ; a8 01
    je short 029fch                           ; 74 0b
    mov dx, word [bp-010h]                    ; 8b 56 f0
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    jmp short 02998h                          ; eb 9c
    mov al, dl                                ; 88 d0
    and AL, strict byte 0c9h                  ; 24 c9
    cmp AL, strict byte 048h                  ; 3c 48
    je short 02a0fh                           ; 74 0b
    mov dx, word [bp-010h]                    ; 8b 56 f0
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    jmp short 029ach                          ; eb 9d
    mov ax, word [bp+00ch]                    ; 8b 46 0c
    shr ax, 004h                              ; c1 e8 04
    mov dx, word [bp+00eh]                    ; 8b 56 0e
    add dx, ax                                ; 01 c2
    mov ax, word [bp+00ch]                    ; 8b 46 0c
    and ax, strict word 0000fh                ; 25 0f 00
    mov word [bp+00ch], ax                    ; 89 46 0c
    mov word [bp+00eh], dx                    ; 89 56 0e
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
    mov word [bp-014h], cx                    ; 89 4e ec
    mov ax, word [bp+004h]                    ; 8b 46 04
    cmp ax, cx                                ; 39 c8
    jbe short 02a4fh                          ; 76 0c
    mov ax, cx                                ; 89 c8
    sub word [bp+004h], cx                    ; 29 4e 04
    xor ax, cx                                ; 31 c8
    mov word [bp-014h], ax                    ; 89 46 ec
    jmp short 02a59h                          ; eb 0a
    mov cx, ax                                ; 89 c1
    mov word [bp+004h], strict word 00000h    ; c7 46 04 00 00
    sub word [bp-014h], ax                    ; 29 46 ec
    xor ax, ax                                ; 31 c0
    cmp word [bp+008h], strict byte 00000h    ; 83 7e 08 00
    jne short 02a82h                          ; 75 21
    mov dx, word [bp-014h]                    ; 8b 56 ec
    cmp dx, word [bp+006h]                    ; 3b 56 06
    jbe short 02a82h                          ; 76 19
    mov ax, word [bp-014h]                    ; 8b 46 ec
    sub ax, word [bp+006h]                    ; 2b 46 06
    mov word [bp-00ch], ax                    ; 89 46 f4
    mov ax, word [bp+006h]                    ; 8b 46 06
    mov word [bp-014h], ax                    ; 89 46 ec
    xor ax, ax                                ; 31 c0
    mov word [bp+006h], ax                    ; 89 46 06
    mov word [bp+008h], ax                    ; 89 46 08
    jmp short 02a8eh                          ; eb 0c
    mov word [bp-00ch], ax                    ; 89 46 f4
    mov dx, word [bp-014h]                    ; 8b 56 ec
    sub word [bp+006h], dx                    ; 29 56 06
    sbb word [bp+008h], ax                    ; 19 46 08
    mov si, word [bp-014h]                    ; 8b 76 ec
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    test cl, 003h                             ; f6 c1 03
    je short 02a9bh                           ; 74 02
    xor al, al                                ; 30 c0
    test byte [bp-014h], 003h                 ; f6 46 ec 03
    je short 02aa3h                           ; 74 02
    xor al, al                                ; 30 c0
    test byte [bp-00ch], 003h                 ; f6 46 f4 03
    je short 02aabh                           ; 74 02
    xor al, al                                ; 30 c0
    test byte [bp-014h], 001h                 ; f6 46 ec 01
    je short 02ac3h                           ; 74 12
    inc word [bp-014h]                        ; ff 46 ec
    cmp word [bp-00ch], strict byte 00000h    ; 83 7e f4 00
    jbe short 02ac3h                          ; 76 09
    test byte [bp-00ch], 001h                 ; f6 46 f4 01
    je short 02ac3h                           ; 74 03
    dec word [bp-00ch]                        ; ff 4e f4
    cmp AL, strict byte 001h                  ; 3c 01
    jne short 02ad4h                          ; 75 0d
    shr word [bp-014h], 002h                  ; c1 6e ec 02
    shr cx, 002h                              ; c1 e9 02
    shr word [bp-00ch], 002h                  ; c1 6e f4 02
    jmp short 02adch                          ; eb 08
    shr word [bp-014h], 1                     ; d1 6e ec
    shr cx, 1                                 ; d1 e9
    shr word [bp-00ch], 1                     ; d1 6e f4
    cmp AL, strict byte 001h                  ; 3c 01
    jne short 02b0ch                          ; 75 2c
    test cx, cx                               ; 85 c9
    je short 02aeeh                           ; 74 0a
    mov dx, bx                                ; 89 da
    push eax                                  ; 66 50
    in eax, DX                                ; 66 ed
    loop 02ae8h                               ; e2 fc
    pop eax                                   ; 66 58
    mov dx, bx                                ; 89 da
    mov cx, word [bp-014h]                    ; 8b 4e ec
    les di, [bp+00ch]                         ; c4 7e 0c
    db  0f3h, 066h, 06dh
    ; rep insd                                  ; f3 66 6d
    mov ax, word [bp-00ch]                    ; 8b 46 f4
    test ax, ax                               ; 85 c0
    je short 02b2bh                           ; 74 2b
    mov cx, ax                                ; 89 c1
    push eax                                  ; 66 50
    in eax, DX                                ; 66 ed
    loop 02b04h                               ; e2 fc
    pop eax                                   ; 66 58
    jmp short 02b2bh                          ; eb 1f
    test cx, cx                               ; 85 c9
    je short 02b15h                           ; 74 05
    mov dx, bx                                ; 89 da
    in ax, DX                                 ; ed
    loop 02b12h                               ; e2 fd
    mov dx, bx                                ; 89 da
    mov cx, word [bp-014h]                    ; 8b 4e ec
    les di, [bp+00ch]                         ; c4 7e 0c
    rep insw                                  ; f3 6d
    mov ax, word [bp-00ch]                    ; 8b 46 f4
    test ax, ax                               ; 85 c0
    je short 02b2bh                           ; 74 05
    mov cx, ax                                ; 89 c1
    in ax, DX                                 ; ed
    loop 02b28h                               ; e2 fd
    add word [bp+00ch], si                    ; 01 76 0c
    xor ax, ax                                ; 31 c0
    add word [bp-018h], si                    ; 01 76 e8
    adc word [bp-016h], ax                    ; 11 46 ea
    mov ax, word [bp-018h]                    ; 8b 46 e8
    mov es, [bp-00eh]                         ; 8e 46 f2
    mov si, word [bp-012h]                    ; 8b 76 ee
    mov word [es:si+016h], ax                 ; 26 89 44 16
    mov ax, word [bp-016h]                    ; 8b 46 ea
    mov word [es:si+018h], ax                 ; 26 89 44 18
    jmp near 029dbh                           ; e9 8e fe
    mov al, dl                                ; 88 d0
    and AL, strict byte 0e9h                  ; 24 e9
    cmp AL, strict byte 040h                  ; 3c 40
    je short 02b61h                           ; 74 0c
    mov dx, word [bp-010h]                    ; 8b 56 f0
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    jmp near 029ach                           ; e9 4b fe
    mov dx, word [bp-010h]                    ; 8b 56 f0
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    xor dx, dx                                ; 31 d2
    mov ax, dx                                ; 89 d0
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn 0000ch                               ; c2 0c 00
ata_soft_reset_:                             ; 0xf2b77 LB 0x80
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push ax                                   ; 50
    mov bx, ax                                ; 89 c3
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 e3 ea
    mov dx, bx                                ; 89 da
    shr dx, 1                                 ; d1 ea
    and bl, 001h                              ; 80 e3 01
    mov byte [bp-008h], bl                    ; 88 5e f8
    xor dh, dh                                ; 30 f6
    imul bx, dx, strict byte 00006h           ; 6b da 06
    mov es, ax                                ; 8e c0
    add bx, 00122h                            ; 81 c3 22 01
    mov cx, word [es:bx+001c2h]               ; 26 8b 8f c2 01
    mov bx, word [es:bx+001c4h]               ; 26 8b 9f c4 01
    lea dx, [bx+006h]                         ; 8d 57 06
    mov AL, strict byte 00ah                  ; b0 0a
    out DX, AL                                ; ee
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00
    je short 02bb9h                           ; 74 05
    mov ax, 000b0h                            ; b8 b0 00
    jmp short 02bbch                          ; eb 03
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
    jne short 02bcah                          ; 75 f4
    and AL, strict byte 0e9h                  ; 24 e9
    cmp AL, strict byte 040h                  ; 3c 40
    je short 02be7h                           ; 74 0b
    lea dx, [bx+006h]                         ; 8d 57 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov ax, strict word 00001h                ; b8 01 00
    jmp short 02befh                          ; eb 08
    lea dx, [bx+006h]                         ; 8d 57 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    xor ax, ax                                ; 31 c0
    lea sp, [bp-006h]                         ; 8d 66 fa
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
set_diskette_ret_status_:                    ; 0xf2bf7 LB 0x18
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    push dx                                   ; 52
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 00041h                ; ba 41 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 56 ea
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
set_diskette_current_cyl_:                   ; 0xf2c0f LB 0x2d
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    mov bl, al                                ; 88 c3
    cmp AL, strict byte 001h                  ; 3c 01
    jbe short 02c24h                          ; 76 0b
    push 00230h                               ; 68 30 02
    push strict byte 00007h                   ; 6a 07
    call 01972h                               ; e8 51 ed
    add sp, strict byte 00004h                ; 83 c4 04
    movzx ax, dl                              ; 0f b6 c2
    movzx dx, bl                              ; 0f b6 d3
    add dx, 00094h                            ; 81 c2 94 00
    mov bx, ax                                ; 89 c3
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 28 ea
    lea sp, [bp-002h]                         ; 8d 66 fe
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
floppy_wait_for_interrupt_:                  ; 0xf2c3c LB 0x21
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push dx                                   ; 52
    cli                                       ; fa
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01650h                               ; e8 06 ea
    test AL, strict byte 080h                 ; a8 80
    je short 02c52h                           ; 74 04
    and AL, strict byte 080h                  ; 24 80
    jmp short 02c57h                          ; eb 05
    sti                                       ; fb
    hlt                                       ; f4
    cli                                       ; fa
    jmp short 02c41h                          ; eb ea
    lea sp, [bp-002h]                         ; 8d 66 fe
    pop dx                                    ; 5a
    pop bp                                    ; 5d
    retn                                      ; c3
floppy_wait_for_interrupt_or_timeout_:       ; 0xf2c5d LB 0x46
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    cli                                       ; fa
    mov dx, strict word 00040h                ; ba 40 00
    mov ax, dx                                ; 89 d0
    call 01650h                               ; e8 e4 e9
    test al, al                               ; 84 c0
    jne short 02c75h                          ; 75 05
    sti                                       ; fb
    xor cl, cl                                ; 30 c9
    jmp short 02c99h                          ; eb 24
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01650h                               ; e8 d2 e9
    mov cl, al                                ; 88 c1
    test AL, strict byte 080h                 ; a8 80
    je short 02c94h                           ; 74 10
    and AL, strict byte 07fh                  ; 24 7f
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 cc e9
    jmp short 02c99h                          ; eb 05
    sti                                       ; fb
    hlt                                       ; f4
    cli                                       ; fa
    jmp short 02c64h                          ; eb cb
    mov al, cl                                ; 88 c8
    lea sp, [bp-006h]                         ; 8d 66 fa
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
floppy_reset_controller_:                    ; 0xf2ca3 LB 0x2b
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    push dx                                   ; 52
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
    jne short 02cbbh                          ; 75 f4
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
floppy_prepare_controller_:                  ; 0xf2cce LB 0x81
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push ax                                   ; 50
    mov cx, ax                                ; 89 c1
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01650h                               ; e8 70 e9
    and AL, strict byte 07fh                  ; 24 7f
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 70 e9
    mov dx, 003f2h                            ; ba f2 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and AL, strict byte 004h                  ; 24 04
    mov byte [bp-008h], al                    ; 88 46 f8
    test cx, cx                               ; 85 c9
    je short 02d01h                           ; 74 04
    mov AL, strict byte 020h                  ; b0 20
    jmp short 02d03h                          ; eb 02
    mov AL, strict byte 010h                  ; b0 10
    or AL, strict byte 00ch                   ; 0c 0c
    or al, cl                                 ; 08 c8
    mov dx, 003f2h                            ; ba f2 03
    out DX, AL                                ; ee
    mov bx, strict word 00025h                ; bb 25 00
    mov dx, strict word 00040h                ; ba 40 00
    mov ax, dx                                ; 89 d0
    call 0165eh                               ; e8 48 e9
    mov dx, 0008bh                            ; ba 8b 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01650h                               ; e8 31 e9
    shr al, 006h                              ; c0 e8 06
    mov dx, 003f7h                            ; ba f7 03
    out DX, AL                                ; ee
    mov dx, 003f4h                            ; ba f4 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and AL, strict byte 0c0h                  ; 24 c0
    cmp AL, strict byte 080h                  ; 3c 80
    jne short 02d26h                          ; 75 f4
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00
    jne short 02d47h                          ; 75 0f
    call 02c3ch                               ; e8 01 ff
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 17 e9
    lea sp, [bp-006h]                         ; 8d 66 fa
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
floppy_media_known_:                         ; 0xf2d4f LB 0x43
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    push dx                                   ; 52
    mov bx, ax                                ; 89 c3
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01650h                               ; e8 f1 e8
    mov ah, al                                ; 88 c4
    test bx, bx                               ; 85 db
    je short 02d67h                           ; 74 02
    shr al, 1                                 ; d0 e8
    and AL, strict byte 001h                  ; 24 01
    jne short 02d6fh                          ; 75 04
    xor ah, ah                                ; 30 e4
    jmp short 02d8bh                          ; eb 1c
    mov dx, 00090h                            ; ba 90 00
    test bx, bx                               ; 85 db
    je short 02d79h                           ; 74 03
    mov dx, 00091h                            ; ba 91 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01650h                               ; e8 d1 e8
    xor ah, ah                                ; 30 e4
    sar ax, 004h                              ; c1 f8 04
    and AL, strict byte 001h                  ; 24 01
    je short 02d6bh                           ; 74 e3
    mov ax, strict word 00001h                ; b8 01 00
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
floppy_read_id_:                             ; 0xf2d92 LB 0x40
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    push dx                                   ; 52
    push si                                   ; 56
    sub sp, strict byte 00008h                ; 83 ec 08
    mov bx, ax                                ; 89 c3
    call 02cceh                               ; e8 2e ff
    mov AL, strict byte 04ah                  ; b0 4a
    mov dx, 003f5h                            ; ba f5 03
    out DX, AL                                ; ee
    mov al, bl                                ; 88 d8
    out DX, AL                                ; ee
    call 02c3ch                               ; e8 90 fe
    xor si, si                                ; 31 f6
    jmp short 02db5h                          ; eb 05
    cmp si, strict byte 00007h                ; 83 fe 07
    jnl short 02dc1h                          ; 7d 0c
    mov dx, 003f5h                            ; ba f5 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov byte [bp+si-00eh], al                 ; 88 42 f2
    inc si                                    ; 46
    jmp short 02db0h                          ; eb ef
    test byte [bp-00eh], 0c0h                 ; f6 46 f2 c0
    db  00fh, 094h, 0c0h
    ; sete al                                   ; 0f 94 c0
    xor ah, ah                                ; 30 e4
    lea sp, [bp-006h]                         ; 8d 66 fa
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
floppy_drive_recal_:                         ; 0xf2dd2 LB 0x48
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    mov bx, ax                                ; 89 c3
    call 02cceh                               ; e8 f1 fe
    mov AL, strict byte 007h                  ; b0 07
    mov dx, 003f5h                            ; ba f5 03
    out DX, AL                                ; ee
    mov al, bl                                ; 88 d8
    out DX, AL                                ; ee
    call 02c3ch                               ; e8 53 fe
    test bx, bx                               ; 85 db
    je short 02df4h                           ; 74 07
    or AL, strict byte 002h                   ; 0c 02
    mov cx, 00095h                            ; b9 95 00
    jmp short 02df9h                          ; eb 05
    or AL, strict byte 001h                   ; 0c 01
    mov cx, 00094h                            ; b9 94 00
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 59 e8
    xor bx, bx                                ; 31 db
    mov dx, cx                                ; 89 ca
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 4f e8
    mov ax, strict word 00001h                ; b8 01 00
    lea sp, [bp-006h]                         ; 8d 66 fa
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
floppy_media_sense_:                         ; 0xf2e1a LB 0xf0
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push si                                   ; 56
    push di                                   ; 57
    mov di, ax                                ; 89 c7
    call 02dd2h                               ; e8 ab ff
    test ax, ax                               ; 85 c0
    jne short 02e30h                          ; 75 05
    xor cx, cx                                ; 31 c9
    jmp near 02efeh                           ; e9 ce 00
    mov ax, strict word 00010h                ; b8 10 00
    call 016ach                               ; e8 76 e8
    test di, di                               ; 85 ff
    jne short 02e41h                          ; 75 07
    mov cl, al                                ; 88 c1
    shr cl, 004h                              ; c0 e9 04
    jmp short 02e46h                          ; eb 05
    mov cl, al                                ; 88 c1
    and cl, 00fh                              ; 80 e1 0f
    cmp cl, 001h                              ; 80 f9 01
    jne short 02e54h                          ; 75 09
    xor cl, cl                                ; 30 c9
    mov CH, strict byte 015h                  ; b5 15
    mov si, strict word 00001h                ; be 01 00
    jmp short 02e92h                          ; eb 3e
    cmp cl, 002h                              ; 80 f9 02
    jne short 02e5fh                          ; 75 06
    xor cl, cl                                ; 30 c9
    mov CH, strict byte 035h                  ; b5 35
    jmp short 02e4fh                          ; eb f0
    cmp cl, 003h                              ; 80 f9 03
    jne short 02e6ah                          ; 75 06
    xor cl, cl                                ; 30 c9
    mov CH, strict byte 017h                  ; b5 17
    jmp short 02e4fh                          ; eb e5
    cmp cl, 004h                              ; 80 f9 04
    jne short 02e75h                          ; 75 06
    xor cl, cl                                ; 30 c9
    mov CH, strict byte 017h                  ; b5 17
    jmp short 02e4fh                          ; eb da
    cmp cl, 005h                              ; 80 f9 05
    jne short 02e80h                          ; 75 06
    mov CL, strict byte 0cch                  ; b1 cc
    mov CH, strict byte 0d7h                  ; b5 d7
    jmp short 02e4fh                          ; eb cf
    cmp cl, 00eh                              ; 80 f9 0e
    je short 02e8ah                           ; 74 05
    cmp cl, 00fh                              ; 80 f9 0f
    jne short 02e8ch                          ; 75 02
    jmp short 02e7ah                          ; eb ee
    xor cl, cl                                ; 30 c9
    xor ch, ch                                ; 30 ed
    xor si, si                                ; 31 f6
    movzx bx, cl                              ; 0f b6 d9
    mov dx, 0008bh                            ; ba 8b 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 c0 e7
    mov ax, di                                ; 89 f8
    call 02d92h                               ; e8 ef fe
    test ax, ax                               ; 85 c0
    jne short 02ed9h                          ; 75 32
    mov al, cl                                ; 88 c8
    and AL, strict byte 0c0h                  ; 24 c0
    cmp AL, strict byte 080h                  ; 3c 80
    je short 02ed9h                           ; 74 2a
    mov al, cl                                ; 88 c8
    and AL, strict byte 0c0h                  ; 24 c0
    cmp AL, strict byte 0c0h                  ; 3c c0
    je short 02ec6h                           ; 74 0f
    mov ah, cl                                ; 88 cc
    and ah, 03fh                              ; 80 e4 3f
    cmp AL, strict byte 040h                  ; 3c 40
    je short 02ed2h                           ; 74 12
    test al, al                               ; 84 c0
    je short 02ecbh                           ; 74 07
    jmp short 02e92h                          ; eb cc
    and cl, 03fh                              ; 80 e1 3f
    jmp short 02e92h                          ; eb c7
    mov cl, ah                                ; 88 e1
    or cl, 040h                               ; 80 c9 40
    jmp short 02e92h                          ; eb c0
    mov cl, ah                                ; 88 e1
    or cl, 080h                               ; 80 c9 80
    jmp short 02e92h                          ; eb b9
    test di, di                               ; 85 ff
    db  00fh, 095h, 0c0h
    ; setne al                                  ; 0f 95 c0
    movzx di, al                              ; 0f b6 f8
    add di, 00090h                            ; 81 c7 90 00
    movzx bx, cl                              ; 0f b6 d9
    mov dx, 0008bh                            ; ba 8b 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 6d e7
    movzx bx, ch                              ; 0f b6 dd
    mov dx, di                                ; 89 fa
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 62 e7
    mov cx, si                                ; 89 f1
    mov ax, cx                                ; 89 c8
    lea sp, [bp-00ah]                         ; 8d 66 f6
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
floppy_drive_exists_:                        ; 0xf2f0a LB 0x24
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push dx                                   ; 52
    mov dx, ax                                ; 89 c2
    mov ax, strict word 00010h                ; b8 10 00
    call 016ach                               ; e8 96 e7
    test dx, dx                               ; 85 d2
    jne short 02f1fh                          ; 75 05
    shr al, 004h                              ; c0 e8 04
    jmp short 02f21h                          ; eb 02
    and AL, strict byte 00fh                  ; 24 0f
    test al, al                               ; 84 c0
    db  00fh, 095h, 0c0h
    ; setne al                                  ; 0f 95 c0
    xor ah, ah                                ; 30 e4
    lea sp, [bp-002h]                         ; 8d 66 fe
    pop dx                                    ; 5a
    pop bp                                    ; 5d
    retn                                      ; c3
_int13_diskette_function:                    ; 0xf2f2e LB 0x734
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    push di                                   ; 57
    sub sp, strict byte 00012h                ; 83 ec 12
    mov bx, word [bp+016h]                    ; 8b 5e 16
    shr bx, 008h                              ; c1 eb 08
    mov ch, bl                                ; 88 dd
    mov si, word [bp+016h]                    ; 8b 76 16
    and si, 000ffh                            ; 81 e6 ff 00
    mov ah, byte [bp+00eh]                    ; 8a 66 0e
    cmp bl, 008h                              ; 80 fb 08
    jc short 02f85h                           ; 72 38
    mov dx, word [bp+01ch]                    ; 8b 56 1c
    or dl, 001h                               ; 80 ca 01
    cmp bl, 008h                              ; 80 fb 08
    jbe near 034e1h                           ; 0f 86 87 05
    cmp bl, 016h                              ; 80 fb 16
    jc short 02f7bh                           ; 72 1c
    or si, 00100h                             ; 81 ce 00 01
    cmp bl, 016h                              ; 80 fb 16
    jbe near 0361dh                           ; 0f 86 b3 06
    cmp bl, 018h                              ; 80 fb 18
    je near 03622h                            ; 0f 84 b1 06
    cmp bl, 017h                              ; 80 fb 17
    je near 03622h                            ; 0f 84 aa 06
    jmp near 0363fh                           ; e9 c4 06
    cmp bl, 015h                              ; 80 fb 15
    je near 035d7h                            ; 0f 84 55 06
    jmp near 0363fh                           ; e9 ba 06
    cmp bl, 001h                              ; 80 fb 01
    jc short 02f9fh                           ; 72 15
    jbe near 03018h                           ; 0f 86 8a 00
    cmp bl, 005h                              ; 80 fb 05
    je near 0335bh                            ; 0f 84 c6 03
    cmp bl, 004h                              ; 80 fb 04
    jbe near 03036h                           ; 0f 86 9a 00
    jmp near 0363fh                           ; e9 a0 06
    test bl, bl                               ; 84 db
    jne near 0363fh                           ; 0f 85 9a 06
    mov al, byte [bp+00eh]                    ; 8a 46 0e
    mov byte [bp-00eh], al                    ; 88 46 f2
    cmp AL, strict byte 001h                  ; 3c 01
    jbe short 02fc3h                          ; 76 14
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    or ah, 001h                               ; 80 cc 01
    mov word [bp+016h], ax                    ; 89 46 16
    mov ax, strict word 00001h                ; b8 01 00
    call 02bf7h                               ; e8 37 fc
    jmp near 03337h                           ; e9 74 03
    mov ax, strict word 00010h                ; b8 10 00
    call 016ach                               ; e8 e3 e6
    cmp byte [bp-00eh], 000h                  ; 80 7e f2 00
    jne short 02fd6h                          ; 75 07
    mov dl, al                                ; 88 c2
    shr dl, 004h                              ; c0 ea 04
    jmp short 02fdbh                          ; eb 05
    mov dl, al                                ; 88 c2
    and dl, 00fh                              ; 80 e2 0f
    test dl, dl                               ; 84 d2
    jne short 02fefh                          ; 75 10
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    or ah, 080h                               ; 80 cc 80
    mov word [bp+016h], ax                    ; 89 46 16
    mov ax, 00080h                            ; b8 80 00
    jmp short 02fbdh                          ; eb ce
    xor bx, bx                                ; 31 db
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 64 e6
    xor al, al                                ; 30 c0
    mov byte [bp+017h], al                    ; 88 46 17
    xor ah, ah                                ; 30 e4
    call 02bf7h                               ; e8 f3 fb
    and byte [bp+01ch], 0feh                  ; 80 66 1c fe
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2
    xor dx, dx                                ; 31 d2
    call 02c0fh                               ; e8 fe fb
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn                                      ; c3
    and byte [bp+01ch], 0feh                  ; 80 66 1c fe
    mov dx, 00441h                            ; ba 41 04
    xor ax, ax                                ; 31 c0
    call 01650h                               ; e8 2c e6
    movzx dx, al                              ; 0f b6 d0
    sal dx, 008h                              ; c1 e2 08
    or si, dx                                 ; 09 d6
    mov word [bp+016h], si                    ; 89 76 16
    test al, al                               ; 84 c0
    je short 03011h                           ; 74 de
    jmp near 03337h                           ; e9 01 03
    mov al, byte [bp+016h]                    ; 8a 46 16
    mov byte [bp-00ch], al                    ; 88 46 f4
    mov dx, word [bp+014h]                    ; 8b 56 14
    shr dx, 008h                              ; c1 ea 08
    mov byte [bp-006h], dl                    ; 88 56 fa
    mov al, byte [bp+014h]                    ; 8a 46 14
    mov byte [bp-00ah], al                    ; 88 46 f6
    mov dx, word [bp+012h]                    ; 8b 56 12
    shr dx, 008h                              ; c1 ea 08
    mov byte [bp-008h], dl                    ; 88 56 f8
    mov byte [bp-00eh], ah                    ; 88 66 f2
    cmp ah, 001h                              ; 80 fc 01
    jnbe short 0306ch                         ; 77 10
    cmp dl, 001h                              ; 80 fa 01
    jnbe short 0306ch                         ; 77 0b
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    test al, al                               ; 84 c0
    je short 0306ch                           ; 74 04
    cmp AL, strict byte 048h                  ; 3c 48
    jbe short 03095h                          ; 76 29
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 ba e8
    push 00255h                               ; 68 55 02
    push 0026dh                               ; 68 6d 02
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 f0 e8
    add sp, strict byte 00006h                ; 83 c4 06
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    or ah, 001h                               ; 80 cc 01
    mov word [bp+016h], ax                    ; 89 46 16
    mov ax, strict word 00001h                ; b8 01 00
    jmp short 03106h                          ; eb 71
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2
    call 02f0ah                               ; e8 6e fe
    test ax, ax                               ; 85 c0
    je near 03199h                            ; 0f 84 f7 00
    movzx dx, byte [bp-00eh]                  ; 0f b6 56 f2
    mov ax, dx                                ; 89 d0
    call 02d4fh                               ; e8 a4 fc
    test ax, ax                               ; 85 c0
    jne short 030c8h                          ; 75 19
    mov ax, dx                                ; 89 d0
    call 02e1ah                               ; e8 66 fd
    test ax, ax                               ; 85 c0
    jne short 030c8h                          ; 75 10
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    or ah, 00ch                               ; 80 cc 0c
    mov word [bp+016h], ax                    ; 89 46 16
    mov ax, strict word 0000ch                ; b8 0c 00
    jmp short 03106h                          ; eb 3e
    cmp ch, 002h                              ; 80 fd 02
    jne near 03230h                           ; 0f 85 61 01
    mov cx, word [bp+006h]                    ; 8b 4e 06
    shr cx, 00ch                              ; c1 e9 0c
    mov ah, cl                                ; 88 cc
    mov dx, word [bp+006h]                    ; 8b 56 06
    sal dx, 004h                              ; c1 e2 04
    mov bx, word [bp+010h]                    ; 8b 5e 10
    add bx, dx                                ; 01 d3
    cmp bx, dx                                ; 39 d3
    jnc short 030e8h                          ; 73 02
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4
    movzx cx, byte [bp-00ch]                  ; 0f b6 4e f4
    sal cx, 009h                              ; c1 e1 09
    dec cx                                    ; 49
    mov dx, bx                                ; 89 da
    add dx, cx                                ; 01 ca
    cmp dx, bx                                ; 39 da
    jnc short 03110h                          ; 73 18
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    or ah, 009h                               ; 80 cc 09
    mov word [bp+016h], ax                    ; 89 46 16
    mov ax, strict word 00009h                ; b8 09 00
    call 02bf7h                               ; e8 ee fa
    mov byte [bp+016h], 000h                  ; c6 46 16 00
    jmp near 03337h                           ; e9 27 02
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
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2
    call 02cceh                               ; e8 7a fb
    mov AL, strict byte 0e6h                  ; b0 e6
    mov dx, 003f5h                            ; ba f5 03
    out DX, AL                                ; ee
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    sal ax, 002h                              ; c1 e0 02
    movzx dx, byte [bp-00eh]                  ; 0f b6 56 f2
    or ax, dx                                 ; 09 d0
    mov dx, 003f5h                            ; ba f5 03
    out DX, AL                                ; ee
    mov al, byte [bp-006h]                    ; 8a 46 fa
    out DX, AL                                ; ee
    mov al, byte [bp-008h]                    ; 8a 46 f8
    out DX, AL                                ; ee
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    out DX, AL                                ; ee
    mov AL, strict byte 002h                  ; b0 02
    out DX, AL                                ; ee
    movzx dx, byte [bp-00ah]                  ; 0f b6 56 f6
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4
    add ax, dx                                ; 01 d0
    dec ax                                    ; 48
    mov dx, 003f5h                            ; ba f5 03
    out DX, AL                                ; ee
    xor al, al                                ; 30 c0
    out DX, AL                                ; ee
    mov AL, strict byte 0ffh                  ; b0 ff
    out DX, AL                                ; ee
    call 02c5dh                               ; e8 cb fa
    test al, al                               ; 84 c0
    jne short 031aah                          ; 75 14
    call 02ca3h                               ; e8 0a fb
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    or ah, 080h                               ; 80 cc 80
    mov word [bp+016h], ax                    ; 89 46 16
    mov ax, 00080h                            ; b8 80 00
    jmp near 03106h                           ; e9 5c ff
    mov dx, 003f4h                            ; ba f4 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and AL, strict byte 0c0h                  ; 24 c0
    cmp AL, strict byte 0c0h                  ; 3c c0
    je short 031c4h                           ; 74 0e
    push 00255h                               ; 68 55 02
    push 00288h                               ; 68 88 02
    push strict byte 00007h                   ; 6a 07
    call 01972h                               ; e8 b1 e7
    add sp, strict byte 00006h                ; 83 c4 06
    xor si, si                                ; 31 f6
    jmp short 031cdh                          ; eb 05
    cmp si, strict byte 00007h                ; 83 fe 07
    jnl short 031e5h                          ; 7d 18
    mov dx, 003f5h                            ; ba f5 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov byte [bp+si-016h], al                 ; 88 42 ea
    movzx bx, al                              ; 0f b6 d8
    lea dx, [si+042h]                         ; 8d 54 42
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 7c e4
    inc si                                    ; 46
    jmp short 031c8h                          ; eb e3
    test byte [bp-016h], 0c0h                 ; f6 46 ea c0
    je short 031fch                           ; 74 11
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    or ah, 020h                               ; 80 cc 20
    mov word [bp+016h], ax                    ; 89 46 16
    mov ax, strict word 00020h                ; b8 20 00
    jmp near 03106h                           ; e9 0a ff
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4
    sal ax, 009h                              ; c1 e0 09
    cwd                                       ; 99
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2
    sar ax, 1                                 ; d1 f8
    mov si, word [bp+010h]                    ; 8b 76 10
    mov dx, word [bp+006h]                    ; 8b 56 06
    mov di, si                                ; 89 f7
    mov es, dx                                ; 8e c2
    mov cx, ax                                ; 89 c1
    push DS                                   ; 1e
    mov ds, dx                                ; 8e da
    rep movsw                                 ; f3 a5
    pop DS                                    ; 1f
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2
    call 02c0fh                               ; e8 ea f9
    mov byte [bp+017h], 000h                  ; c6 46 17 00
    and byte [bp+01ch], 0feh                  ; 80 66 1c fe
    jmp near 03011h                           ; e9 e1 fd
    cmp ch, 003h                              ; 80 fd 03
    jne near 03345h                           ; 0f 85 0e 01
    mov dx, word [bp+006h]                    ; 8b 56 06
    shr dx, 00ch                              ; c1 ea 0c
    mov ah, dl                                ; 88 d4
    mov cx, word [bp+006h]                    ; 8b 4e 06
    sal cx, 004h                              ; c1 e1 04
    mov bx, word [bp+010h]                    ; 8b 5e 10
    add bx, cx                                ; 01 cb
    cmp bx, cx                                ; 39 cb
    jnc short 03250h                          ; 73 02
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4
    movzx cx, byte [bp-00ch]                  ; 0f b6 4e f4
    sal cx, 009h                              ; c1 e1 09
    dec cx                                    ; 49
    mov dx, bx                                ; 89 da
    add dx, cx                                ; 01 ca
    cmp dx, bx                                ; 39 da
    jc near 030f8h                            ; 0f 82 96 fe
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
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2
    call 02cceh                               ; e8 29 fa
    mov AL, strict byte 0c5h                  ; b0 c5
    mov dx, 003f5h                            ; ba f5 03
    out DX, AL                                ; ee
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8
    sal dx, 002h                              ; c1 e2 02
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2
    or ax, dx                                 ; 09 d0
    mov dx, 003f5h                            ; ba f5 03
    out DX, AL                                ; ee
    mov al, byte [bp-006h]                    ; 8a 46 fa
    out DX, AL                                ; ee
    mov al, byte [bp-008h]                    ; 8a 46 f8
    out DX, AL                                ; ee
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    out DX, AL                                ; ee
    mov AL, strict byte 002h                  ; b0 02
    out DX, AL                                ; ee
    movzx dx, byte [bp-00ah]                  ; 0f b6 56 f6
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4
    add ax, dx                                ; 01 d0
    dec ax                                    ; 48
    mov dx, 003f5h                            ; ba f5 03
    out DX, AL                                ; ee
    xor al, al                                ; 30 c0
    out DX, AL                                ; ee
    mov AL, strict byte 0ffh                  ; b0 ff
    out DX, AL                                ; ee
    call 02c5dh                               ; e8 7a f9
    test al, al                               ; 84 c0
    je near 03196h                            ; 0f 84 ad fe
    mov dx, 003f4h                            ; ba f4 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and AL, strict byte 0c0h                  ; 24 c0
    cmp AL, strict byte 0c0h                  ; 3c c0
    je short 03303h                           ; 74 0e
    push 00255h                               ; 68 55 02
    push 00288h                               ; 68 88 02
    push strict byte 00007h                   ; 6a 07
    call 01972h                               ; e8 72 e6
    add sp, strict byte 00006h                ; 83 c4 06
    xor si, si                                ; 31 f6
    jmp short 0330ch                          ; eb 05
    cmp si, strict byte 00007h                ; 83 fe 07
    jnl short 03324h                          ; 7d 18
    mov dx, 003f5h                            ; ba f5 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov byte [bp+si-016h], al                 ; 88 42 ea
    movzx bx, al                              ; 0f b6 d8
    lea dx, [si+042h]                         ; 8d 54 42
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 3d e3
    inc si                                    ; 46
    jmp short 03307h                          ; eb e3
    test byte [bp-016h], 0c0h                 ; f6 46 ea c0
    je near 0321ah                            ; 0f 84 ee fe
    test byte [bp-015h], 002h                 ; f6 46 eb 02
    je short 0333eh                           ; 74 0c
    mov word [bp+016h], 00300h                ; c7 46 16 00 03
    or byte [bp+01ch], 001h                   ; 80 4e 1c 01
    jmp near 03011h                           ; e9 d3 fc
    mov word [bp+016h], 00100h                ; c7 46 16 00 01
    jmp short 03337h                          ; eb f2
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2
    call 02c0fh                               ; e8 bf f8
    and byte [bp+01ch], 0feh                  ; 80 66 1c fe
    mov byte [bp+017h], 000h                  ; c6 46 17 00
    jmp near 03011h                           ; e9 b6 fc
    mov al, byte [bp+016h]                    ; 8a 46 16
    mov byte [bp-00ch], al                    ; 88 46 f4
    mov dx, word [bp+014h]                    ; 8b 56 14
    shr dx, 008h                              ; c1 ea 08
    mov byte [bp-006h], dl                    ; 88 56 fa
    mov ax, word [bp+012h]                    ; 8b 46 12
    shr ax, 008h                              ; c1 e8 08
    mov byte [bp-008h], al                    ; 88 46 f8
    mov bl, byte [bp+00eh]                    ; 8a 5e 0e
    mov byte [bp-00eh], bl                    ; 88 5e f2
    cmp bl, 001h                              ; 80 fb 01
    jnbe short 03392h                         ; 77 14
    cmp AL, strict byte 001h                  ; 3c 01
    jnbe short 03392h                         ; 77 10
    cmp dl, 04fh                              ; 80 fa 4f
    jnbe short 03392h                         ; 77 0b
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    test al, al                               ; 84 c0
    je short 03392h                           ; 74 04
    cmp AL, strict byte 012h                  ; 3c 12
    jbe short 033a7h                          ; 76 15
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    or ah, 001h                               ; 80 cc 01
    mov word [bp+016h], ax                    ; 89 46 16
    mov ax, strict word 00001h                ; b8 01 00
    call 02bf7h                               ; e8 54 f8
    or byte [bp+01ch], 001h                   ; 80 4e 1c 01
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2
    call 02f0ah                               ; e8 5c fb
    test ax, ax                               ; 85 c0
    je near 02fdfh                            ; 0f 84 2b fc
    movzx dx, byte [bp-00eh]                  ; 0f b6 56 f2
    mov ax, dx                                ; 89 d0
    call 02d4fh                               ; e8 92 f9
    test ax, ax                               ; 85 c0
    jne short 033cch                          ; 75 0b
    mov ax, dx                                ; 89 d0
    call 02e1ah                               ; e8 54 fa
    test ax, ax                               ; 85 c0
    je near 030b8h                            ; 0f 84 ec fc
    mov cx, word [bp+006h]                    ; 8b 4e 06
    shr cx, 00ch                              ; c1 e9 0c
    mov ah, cl                                ; 88 cc
    mov dx, word [bp+006h]                    ; 8b 56 06
    sal dx, 004h                              ; c1 e2 04
    mov bx, word [bp+010h]                    ; 8b 5e 10
    add bx, dx                                ; 01 d3
    cmp bx, dx                                ; 39 d3
    jnc short 033e5h                          ; 73 02
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4
    movzx cx, byte [bp-00ch]                  ; 0f b6 4e f4
    sal cx, 002h                              ; c1 e1 02
    dec cx                                    ; 49
    mov dx, bx                                ; 89 da
    add dx, cx                                ; 01 ca
    cmp dx, bx                                ; 39 da
    jc near 030f8h                            ; 0f 82 01 fd
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
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2
    call 02cceh                               ; e8 94 f8
    mov AL, strict byte 00fh                  ; b0 0f
    mov dx, 003f5h                            ; ba f5 03
    out DX, AL                                ; ee
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    sal ax, 002h                              ; c1 e0 02
    movzx bx, byte [bp-00eh]                  ; 0f b6 5e f2
    or bx, ax                                 ; 09 c3
    mov al, bl                                ; 88 d8
    out DX, AL                                ; ee
    mov al, byte [bp-006h]                    ; 8a 46 fa
    out DX, AL                                ; ee
    mov AL, strict byte 04dh                  ; b0 4d
    out DX, AL                                ; ee
    mov al, bl                                ; 88 d8
    out DX, AL                                ; ee
    mov AL, strict byte 002h                  ; b0 02
    out DX, AL                                ; ee
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    out DX, AL                                ; ee
    xor al, al                                ; 30 c0
    out DX, AL                                ; ee
    mov AL, strict byte 0f6h                  ; b0 f6
    out DX, AL                                ; ee
    call 02c5dh                               ; e8 f3 f7
    test al, al                               ; 84 c0
    jne short 03474h                          ; 75 06
    call 02ca3h                               ; e8 32 f8
    jmp near 02fdfh                           ; e9 6b fb
    mov dx, 003f4h                            ; ba f4 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and AL, strict byte 0c0h                  ; 24 c0
    cmp AL, strict byte 0c0h                  ; 3c c0
    je short 0348eh                           ; 74 0e
    push 00255h                               ; 68 55 02
    push 00288h                               ; 68 88 02
    push strict byte 00007h                   ; 6a 07
    call 01972h                               ; e8 e7 e4
    add sp, strict byte 00006h                ; 83 c4 06
    xor si, si                                ; 31 f6
    jmp short 03497h                          ; eb 05
    cmp si, strict byte 00007h                ; 83 fe 07
    jnl short 034afh                          ; 7d 18
    mov dx, 003f5h                            ; ba f5 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov byte [bp+si-016h], al                 ; 88 42 ea
    movzx bx, al                              ; 0f b6 d8
    lea dx, [si+042h]                         ; 8d 54 42
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 b2 e1
    inc si                                    ; 46
    jmp short 03492h                          ; eb e3
    test byte [bp-016h], 0c0h                 ; f6 46 ea c0
    je short 034cbh                           ; 74 16
    test byte [bp-015h], 002h                 ; f6 46 eb 02
    jne near 03332h                           ; 0f 85 75 fe
    push 00255h                               ; 68 55 02
    push 0029ch                               ; 68 9c 02
    push strict byte 00007h                   ; 6a 07
    call 01972h                               ; e8 aa e4
    add sp, strict byte 00006h                ; 83 c4 06
    xor al, al                                ; 30 c0
    mov byte [bp+017h], al                    ; 88 46 17
    xor ah, ah                                ; 30 e4
    call 02bf7h                               ; e8 22 f7
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2
    xor dx, dx                                ; 31 d2
    call 02c0fh                               ; e8 31 f7
    jmp near 03229h                           ; e9 48 fd
    mov byte [bp-00eh], ah                    ; 88 66 f2
    cmp ah, 001h                              ; 80 fc 01
    jbe short 03509h                          ; 76 20
    xor ax, ax                                ; 31 c0
    mov word [bp+016h], ax                    ; 89 46 16
    mov word [bp+010h], ax                    ; 89 46 10
    mov word [bp+014h], ax                    ; 89 46 14
    mov word [bp+012h], ax                    ; 89 46 12
    mov word [bp+006h], ax                    ; 89 46 06
    mov word [bp+008h], ax                    ; 89 46 08
    movzx ax, cl                              ; 0f b6 c1
    mov word [bp+012h], ax                    ; 89 46 12
    mov word [bp+01ch], dx                    ; 89 56 1c
    jmp near 03011h                           ; e9 08 fb
    mov ax, strict word 00010h                ; b8 10 00
    call 016ach                               ; e8 9d e1
    mov dl, al                                ; 88 c2
    xor cl, cl                                ; 30 c9
    test AL, strict byte 0f0h                 ; a8 f0
    je short 03519h                           ; 74 02
    mov CL, strict byte 001h                  ; b1 01
    test dl, 00fh                             ; f6 c2 0f
    je short 03520h                           ; 74 02
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1
    cmp byte [bp-00eh], 000h                  ; 80 7e f2 00
    jne short 0352bh                          ; 75 05
    shr dl, 004h                              ; c0 ea 04
    jmp short 0352eh                          ; eb 03
    and dl, 00fh                              ; 80 e2 0f
    mov byte [bp+011h], 000h                  ; c6 46 11 00
    movzx ax, dl                              ; 0f b6 c2
    mov word [bp+010h], ax                    ; 89 46 10
    mov word [bp+016h], strict word 00000h    ; c7 46 16 00 00
    mov bx, word [bp+012h]                    ; 8b 5e 12
    xor bl, bl                                ; 30 db
    movzx ax, cl                              ; 0f b6 c1
    or bx, ax                                 ; 09 c3
    mov word [bp+012h], bx                    ; 89 5e 12
    mov ax, bx                                ; 89 d8
    xor ah, bh                                ; 30 fc
    or ah, 001h                               ; 80 cc 01
    mov word [bp+012h], ax                    ; 89 46 12
    cmp dl, 003h                              ; 80 fa 03
    jc short 0356eh                           ; 72 15
    jbe short 03595h                          ; 76 3a
    cmp dl, 005h                              ; 80 fa 05
    jc short 0359ch                           ; 72 3c
    jbe short 035a3h                          ; 76 41
    cmp dl, 00fh                              ; 80 fa 0f
    je short 035b1h                           ; 74 4a
    cmp dl, 00eh                              ; 80 fa 0e
    je short 035aah                           ; 74 3e
    jmp short 035b8h                          ; eb 4a
    cmp dl, 002h                              ; 80 fa 02
    je short 0358eh                           ; 74 1b
    cmp dl, 001h                              ; 80 fa 01
    je short 03587h                           ; 74 0f
    test dl, dl                               ; 84 d2
    jne short 035b8h                          ; 75 3c
    mov word [bp+014h], strict word 00000h    ; c7 46 14 00 00
    mov byte [bp+013h], 000h                  ; c6 46 13 00
    jmp short 035c6h                          ; eb 3f
    mov word [bp+014h], 02709h                ; c7 46 14 09 27
    jmp short 035c6h                          ; eb 38
    mov word [bp+014h], 04f0fh                ; c7 46 14 0f 4f
    jmp short 035c6h                          ; eb 31
    mov word [bp+014h], 04f09h                ; c7 46 14 09 4f
    jmp short 035c6h                          ; eb 2a
    mov word [bp+014h], 04f12h                ; c7 46 14 12 4f
    jmp short 035c6h                          ; eb 23
    mov word [bp+014h], 04f24h                ; c7 46 14 24 4f
    jmp short 035c6h                          ; eb 1c
    mov word [bp+014h], 0fe3fh                ; c7 46 14 3f fe
    jmp short 035c6h                          ; eb 15
    mov word [bp+014h], 0feffh                ; c7 46 14 ff fe
    jmp short 035c6h                          ; eb 0e
    push 00255h                               ; 68 55 02
    push 002adh                               ; 68 ad 02
    push strict byte 00007h                   ; 6a 07
    call 01972h                               ; e8 af e3
    add sp, strict byte 00006h                ; 83 c4 06
    mov word [bp+006h], 0f000h                ; c7 46 06 00 f0
    movzx ax, dl                              ; 0f b6 c2
    call 03662h                               ; e8 91 00
    mov word [bp+008h], ax                    ; 89 46 08
    jmp near 03229h                           ; e9 52 fc
    mov byte [bp-00eh], ah                    ; 88 66 f2
    cmp ah, 001h                              ; 80 fc 01
    jbe short 035e5h                          ; 76 06
    mov word [bp+016h], si                    ; 89 76 16
    jmp near 03503h                           ; e9 1e ff
    mov ax, strict word 00010h                ; b8 10 00
    call 016ach                               ; e8 c1 e0
    cmp byte [bp-00eh], 000h                  ; 80 7e f2 00
    jne short 035f8h                          ; 75 07
    mov dl, al                                ; 88 c2
    shr dl, 004h                              ; c0 ea 04
    jmp short 035fdh                          ; eb 05
    mov dl, al                                ; 88 c2
    and dl, 00fh                              ; 80 e2 0f
    and byte [bp+01ch], 0feh                  ; 80 66 1c fe
    mov bx, word [bp+016h]                    ; 8b 5e 16
    xor bh, bh                                ; 30 ff
    test dl, dl                               ; 84 d2
    je short 03617h                           ; 74 0d
    cmp dl, 001h                              ; 80 fa 01
    jbe short 03614h                          ; 76 05
    or bh, 002h                               ; 80 cf 02
    jmp short 03617h                          ; eb 03
    or bh, 001h                               ; 80 cf 01
    mov word [bp+016h], bx                    ; 89 5e 16
    jmp near 03011h                           ; e9 f4 f9
    cmp ah, 001h                              ; 80 fc 01
    jbe short 0362eh                          ; 76 0c
    mov word [bp+016h], si                    ; 89 76 16
    mov ax, strict word 00001h                ; b8 01 00
    call 02bf7h                               ; e8 cc f5
    jmp near 03503h                           ; e9 d5 fe
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    or ah, 006h                               ; 80 cc 06
    mov word [bp+016h], ax                    ; 89 46 16
    mov ax, strict word 00006h                ; b8 06 00
    jmp near 02fbdh                           ; e9 7e f9
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 e7 e2
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 00255h                               ; 68 55 02
    push 002c2h                               ; 68 c2 02
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 16 e3
    add sp, strict byte 00008h                ; 83 c4 08
    jmp near 02fafh                           ; e9 4d f9
get_floppy_dpt_:                             ; 0xf3662 LB 0x2f
    push bx                                   ; 53
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dl, al                                ; 88 c2
    xor ax, ax                                ; 31 c0
    jmp short 03673h                          ; eb 06
    inc ax                                    ; 40
    cmp ax, strict word 00007h                ; 3d 07 00
    jnc short 0368ah                          ; 73 17
    mov bx, ax                                ; 89 c3
    add bx, ax                                ; 01 c3
    cmp dl, byte [word bx+0005bh]             ; 3a 97 5b 00
    jne short 0366dh                          ; 75 f0
    movzx ax, byte [word bx+0005ch]           ; 0f b6 87 5c 00
    imul ax, ax, strict byte 0000dh           ; 6b c0 0d
    add ax, strict word 00000h                ; 05 00 00
    jmp short 0368dh                          ; eb 03
    mov ax, strict word 00041h                ; b8 41 00
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    retn                                      ; c3
dummy_soft_reset_:                           ; 0xf3691 LB 0x7
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    xor ax, ax                                ; 31 c0
    pop bp                                    ; 5d
    retn                                      ; c3
_cdemu_init:                                 ; 0xf3698 LB 0x18
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 c8 df
    xor bx, bx                                ; 31 db
    mov dx, 00322h                            ; ba 22 03
    call 0165eh                               ; e8 b2 df
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    retn                                      ; c3
_cdemu_isactive:                             ; 0xf36b0 LB 0x16
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 b0 df
    mov dx, 00322h                            ; ba 22 03
    call 01650h                               ; e8 8e df
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    retn                                      ; c3
_cdemu_emulated_drive:                       ; 0xf36c6 LB 0x16
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 9a df
    mov dx, 00324h                            ; ba 24 03
    call 01650h                               ; e8 78 df
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    retn                                      ; c3
_int13_eltorito:                             ; 0xf36dc LB 0x189
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    push di                                   ; 57
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 82 df
    mov si, 00322h                            ; be 22 03
    mov di, ax                                ; 89 c7
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    cmp ax, strict word 0004bh                ; 3d 4b 00
    jc short 03704h                           ; 72 0a
    jbe short 0372ah                          ; 76 2e
    cmp ax, strict word 0004dh                ; 3d 4d 00
    jbe short 0370bh                          ; 76 0a
    jmp near 03829h                           ; e9 25 01
    cmp ax, strict word 0004ah                ; 3d 4a 00
    jne near 03829h                           ; 0f 85 1e 01
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 1b e2
    push word [bp+016h]                       ; ff 76 16
    push 002dch                               ; 68 dc 02
    push 002ebh                               ; 68 eb 02
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 4e e2
    add sp, strict byte 00008h                ; 83 c4 08
    jmp near 03844h                           ; e9 1a 01
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    mov ax, word [bp+004h]                    ; 8b 46 04
    mov bx, strict word 00013h                ; bb 13 00
    call 0165eh                               ; e8 28 df
    mov es, di                                ; 8e c7
    movzx bx, byte [es:si+001h]               ; 26 0f b6 5c 01
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    inc dx                                    ; 42
    mov ax, word [bp+004h]                    ; 8b 46 04
    call 0165eh                               ; e8 17 df
    mov es, di                                ; 8e c7
    movzx bx, byte [es:si+002h]               ; 26 0f b6 5c 02
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    inc dx                                    ; 42
    inc dx                                    ; 42
    mov ax, word [bp+004h]                    ; 8b 46 04
    call 0165eh                               ; e8 05 df
    mov es, di                                ; 8e c7
    movzx bx, byte [es:si+003h]               ; 26 0f b6 5c 03
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 00003h                ; 83 c2 03
    mov ax, word [bp+004h]                    ; 8b 46 04
    call 0165eh                               ; e8 f2 de
    mov es, di                                ; 8e c7
    mov bx, word [es:si+008h]                 ; 26 8b 5c 08
    mov cx, word [es:si+00ah]                 ; 26 8b 4c 0a
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 00004h                ; 83 c2 04
    mov ax, word [bp+004h]                    ; 8b 46 04
    call 0169ah                               ; e8 18 df
    mov es, di                                ; 8e c7
    mov bx, word [es:si+004h]                 ; 26 8b 5c 04
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 00008h                ; 83 c2 08
    mov ax, word [bp+004h]                    ; 8b 46 04
    call 0167ah                               ; e8 e6 de
    mov es, di                                ; 8e c7
    mov bx, word [es:si+006h]                 ; 26 8b 5c 06
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 0000ah                ; 83 c2 0a
    mov ax, word [bp+004h]                    ; 8b 46 04
    call 0167ah                               ; e8 d4 de
    mov es, di                                ; 8e c7
    mov bx, word [es:si+00ch]                 ; 26 8b 5c 0c
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 0000ch                ; 83 c2 0c
    mov ax, word [bp+004h]                    ; 8b 46 04
    call 0167ah                               ; e8 c2 de
    mov es, di                                ; 8e c7
    mov bx, word [es:si+00eh]                 ; 26 8b 5c 0e
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 0000eh                ; 83 c2 0e
    mov ax, word [bp+004h]                    ; 8b 46 04
    call 0167ah                               ; e8 b0 de
    mov es, di                                ; 8e c7
    movzx bx, byte [es:si+012h]               ; 26 0f b6 5c 12
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 00010h                ; 83 c2 10
    mov ax, word [bp+004h]                    ; 8b 46 04
    call 0165eh                               ; e8 81 de
    mov es, di                                ; 8e c7
    movzx bx, byte [es:si+014h]               ; 26 0f b6 5c 14
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 00011h                ; 83 c2 11
    mov ax, word [bp+004h]                    ; 8b 46 04
    call 0165eh                               ; e8 6e de
    mov es, di                                ; 8e c7
    movzx bx, byte [es:si+010h]               ; 26 0f b6 5c 10
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 00012h                ; 83 c2 12
    mov ax, word [bp+004h]                    ; 8b 46 04
    call 0165eh                               ; e8 5b de
    test byte [bp+016h], 0ffh                 ; f6 46 16 ff
    jne short 0380fh                          ; 75 06
    mov es, di                                ; 8e c7
    mov byte [es:si], 000h                    ; 26 c6 04 00
    mov byte [bp+017h], 000h                  ; c6 46 17 00
    xor bx, bx                                ; 31 db
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 40 de
    and byte [bp+01ch], 0feh                  ; 80 66 1c fe
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn                                      ; c3
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 fd e0
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 002dch                               ; 68 dc 02
    push 00313h                               ; 68 13 03
    jmp near 0371fh                           ; e9 db fe
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    or ah, 001h                               ; 80 cc 01
    mov word [bp+016h], ax                    ; 89 46 16
    mov bx, ax                                ; 89 c3
    shr bx, 008h                              ; c1 eb 08
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 ff dd
    or byte [bp+01ch], 001h                   ; 80 4e 1c 01
    jmp short 03822h                          ; eb bd
device_is_cdrom_:                            ; 0xf3865 LB 0x35
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    push dx                                   ; 52
    mov bl, al                                ; 88 c3
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 f7 dd
    cmp bl, 010h                              ; 80 fb 10
    jc short 0387eh                           ; 72 04
    xor ax, ax                                ; 31 c0
    jmp short 03893h                          ; eb 15
    xor bh, bh                                ; 30 ff
    imul bx, bx, strict byte 00018h           ; 6b db 18
    mov es, ax                                ; 8e c0
    add bx, 00122h                            ; 81 c3 22 01
    cmp byte [es:bx+01fh], 005h               ; 26 80 7f 1f 05
    jne short 0387ah                          ; 75 ea
    mov ax, strict word 00001h                ; b8 01 00
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
cdrom_boot_:                                 ; 0xf389a LB 0x416
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push si                                   ; 56
    push di                                   ; 57
    sub sp, 0081ch                            ; 81 ec 1c 08
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 bd dd
    mov word [bp-018h], ax                    ; 89 46 e8
    mov si, 00322h                            ; be 22 03
    mov word [bp-010h], ax                    ; 89 46 f0
    mov word [bp-014h], 00122h                ; c7 46 ec 22 01
    mov word [bp-012h], ax                    ; 89 46 ee
    mov byte [bp-00ch], 000h                  ; c6 46 f4 00
    jmp short 038cfh                          ; eb 09
    inc byte [bp-00ch]                        ; fe 46 f4
    cmp byte [bp-00ch], 010h                  ; 80 7e f4 10
    jnc short 038dah                          ; 73 0b
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4
    call 03865h                               ; e8 8f ff
    test ax, ax                               ; 85 c0
    je short 038c6h                           ; 74 ec
    cmp byte [bp-00ch], 010h                  ; 80 7e f4 10
    jc short 038e6h                           ; 72 06
    mov ax, strict word 00002h                ; b8 02 00
    jmp near 03c4dh                           ; e9 67 03
    mov cx, strict word 0000ch                ; b9 0c 00
    xor bx, bx                                ; 31 db
    mov dx, ss                                ; 8c d2
    lea ax, [bp-026h]                         ; 8d 46 da
    call 094dah                               ; e8 e7 5b
    mov word [bp-026h], strict word 00028h    ; c7 46 da 28 00
    mov ax, strict word 00011h                ; b8 11 00
    xor dx, dx                                ; 31 d2
    xchg ah, al                               ; 86 c4
    xchg dh, dl                               ; 86 d6
    xchg dx, ax                               ; 92
    mov word [bp-024h], ax                    ; 89 46 dc
    mov word [bp-022h], dx                    ; 89 56 de
    mov ax, strict word 00001h                ; b8 01 00
    xchg ah, al                               ; 86 c4
    mov word [bp-01fh], ax                    ; 89 46 e1
    les bx, [bp-014h]                         ; c4 5e ec
    db  066h, 026h, 0c7h, 047h, 00ah, 001h, 000h, 000h, 008h
    ; mov dword [es:bx+00ah], strict dword 008000001h ; 66 26 c7 47 0a 01 00 00 08
    mov byte [bp-00eh], 000h                  ; c6 46 f2 00
    jmp short 0392bh                          ; eb 09
    inc byte [bp-00eh]                        ; fe 46 f2
    cmp byte [bp-00eh], 004h                  ; 80 7e f2 04
    jnbe short 03962h                         ; 77 37
    movzx di, byte [bp-00ch]                  ; 0f b6 7e f4
    imul di, di, strict byte 00018h           ; 6b ff 18
    mov es, [bp-012h]                         ; 8e 46 ee
    add di, word [bp-014h]                    ; 03 7e ec
    movzx di, byte [es:di+01eh]               ; 26 0f b6 7d 1e
    add di, di                                ; 01 ff
    lea dx, [bp-00826h]                       ; 8d 96 da f7
    push SS                                   ; 16
    push dx                                   ; 52
    push strict byte 00001h                   ; 6a 01
    push strict byte 00000h                   ; 6a 00
    push 00800h                               ; 68 00 08
    push strict byte 00000h                   ; 6a 00
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4
    mov cx, ss                                ; 8c d1
    lea bx, [bp-026h]                         ; 8d 5e da
    mov dx, strict word 0000ch                ; ba 0c 00
    call word [word di+0006ah]                ; ff 95 6a 00
    test ax, ax                               ; 85 c0
    jne short 03922h                          ; 75 c0
    test ax, ax                               ; 85 c0
    je short 0396ch                           ; 74 06
    mov ax, strict word 00003h                ; b8 03 00
    jmp near 03c4dh                           ; e9 e1 02
    cmp byte [bp-00826h], 000h                ; 80 be da f7 00
    je short 03979h                           ; 74 06
    mov ax, strict word 00004h                ; b8 04 00
    jmp near 03c4dh                           ; e9 d4 02
    xor di, di                                ; 31 ff
    jmp short 03983h                          ; eb 06
    inc di                                    ; 47
    cmp di, strict byte 00005h                ; 83 ff 05
    jnc short 03993h                          ; 73 10
    mov al, byte [bp+di-00825h]               ; 8a 83 db f7
    cmp al, byte [di+00d92h]                  ; 3a 85 92 0d
    je short 0397dh                           ; 74 f0
    mov ax, strict word 00005h                ; b8 05 00
    jmp near 03c4dh                           ; e9 ba 02
    xor di, di                                ; 31 ff
    jmp short 0399dh                          ; eb 06
    inc di                                    ; 47
    cmp di, strict byte 00017h                ; 83 ff 17
    jnc short 039adh                          ; 73 10
    mov al, byte [bp+di-0081fh]               ; 8a 83 e1 f7
    cmp al, byte [di+00d98h]                  ; 3a 85 98 0d
    je short 03997h                           ; 74 f0
    mov ax, strict word 00006h                ; b8 06 00
    jmp near 03c4dh                           ; e9 a0 02
    mov ax, word [bp-007dfh]                  ; 8b 86 21 f8
    mov dx, word [bp-007ddh]                  ; 8b 96 23 f8
    mov word [bp-026h], strict word 00028h    ; c7 46 da 28 00
    xchg ah, al                               ; 86 c4
    xchg dh, dl                               ; 86 d6
    xchg dx, ax                               ; 92
    mov word [bp-024h], ax                    ; 89 46 dc
    mov word [bp-022h], dx                    ; 89 56 de
    mov ax, strict word 00001h                ; b8 01 00
    xchg ah, al                               ; 86 c4
    mov word [bp-01fh], ax                    ; 89 46 e1
    movzx di, byte [bp-00ch]                  ; 0f b6 7e f4
    imul di, di, strict byte 00018h           ; 6b ff 18
    mov es, [bp-012h]                         ; 8e 46 ee
    add di, word [bp-014h]                    ; 03 7e ec
    movzx di, byte [es:di+01eh]               ; 26 0f b6 7d 1e
    add di, di                                ; 01 ff
    lea dx, [bp-00826h]                       ; 8d 96 da f7
    push SS                                   ; 16
    push dx                                   ; 52
    push strict byte 00001h                   ; 6a 01
    push strict byte 00000h                   ; 6a 00
    push 00800h                               ; 68 00 08
    push strict byte 00000h                   ; 6a 00
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4
    mov cx, ss                                ; 8c d1
    lea bx, [bp-026h]                         ; 8d 5e da
    mov dx, strict word 0000ch                ; ba 0c 00
    call word [word di+0006ah]                ; ff 95 6a 00
    test ax, ax                               ; 85 c0
    je short 03a0ah                           ; 74 06
    mov ax, strict word 00007h                ; b8 07 00
    jmp near 03c4dh                           ; e9 43 02
    cmp byte [bp-00826h], 001h                ; 80 be da f7 01
    je short 03a17h                           ; 74 06
    mov ax, strict word 00008h                ; b8 08 00
    jmp near 03c4dh                           ; e9 36 02
    cmp byte [bp-00825h], 000h                ; 80 be db f7 00
    je short 03a24h                           ; 74 06
    mov ax, strict word 00009h                ; b8 09 00
    jmp near 03c4dh                           ; e9 29 02
    cmp byte [bp-00808h], 055h                ; 80 be f8 f7 55
    je short 03a31h                           ; 74 06
    mov ax, strict word 0000ah                ; b8 0a 00
    jmp near 03c4dh                           ; e9 1c 02
    cmp byte [bp-00807h], 0aah                ; 80 be f9 f7 aa
    jne short 03a2bh                          ; 75 f3
    cmp byte [bp-00806h], 088h                ; 80 be fa f7 88
    je short 03a45h                           ; 74 06
    mov ax, strict word 0000bh                ; b8 0b 00
    jmp near 03c4dh                           ; e9 08 02
    mov al, byte [bp-00805h]                  ; 8a 86 fb f7
    mov es, [bp-010h]                         ; 8e 46 f0
    mov byte [es:si+001h], al                 ; 26 88 44 01
    cmp byte [bp-00805h], 000h                ; 80 be fb f7 00
    jne short 03a5eh                          ; 75 07
    mov byte [es:si+002h], 0e0h               ; 26 c6 44 02 e0
    jmp short 03a71h                          ; eb 13
    cmp byte [bp-00805h], 004h                ; 80 be fb f7 04
    jnc short 03a6ch                          ; 73 07
    mov byte [es:si+002h], 000h               ; 26 c6 44 02 00
    jmp short 03a71h                          ; eb 05
    mov byte [es:si+002h], 080h               ; 26 c6 44 02 80
    movzx di, byte [bp-00ch]                  ; 0f b6 7e f4
    mov ax, di                                ; 89 f8
    cwd                                       ; 99
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2
    sar ax, 1                                 ; d1 f8
    mov es, [bp-010h]                         ; 8e 46 f0
    mov byte [es:si+003h], al                 ; 26 88 44 03
    mov ax, di                                ; 89 f8
    cwd                                       ; 99
    mov bx, strict word 00002h                ; bb 02 00
    idiv bx                                   ; f7 fb
    mov word [es:si+004h], dx                 ; 26 89 54 04
    mov ax, word [bp-00804h]                  ; 8b 86 fc f7
    mov word [bp-016h], ax                    ; 89 46 ea
    test ax, ax                               ; 85 c0
    jne short 03a9fh                          ; 75 05
    mov word [bp-016h], 007c0h                ; c7 46 ea c0 07
    mov ax, word [bp-016h]                    ; 8b 46 ea
    mov es, [bp-010h]                         ; 8e 46 f0
    mov word [es:si+00ch], ax                 ; 26 89 44 0c
    mov word [es:si+006h], strict word 00000h ; 26 c7 44 06 00 00
    mov di, word [bp-00800h]                  ; 8b be 00 f8
    mov word [es:si+00eh], di                 ; 26 89 7c 0e
    test di, di                               ; 85 ff
    je short 03ac1h                           ; 74 06
    cmp di, 00400h                            ; 81 ff 00 04
    jbe short 03ac7h                          ; 76 06
    mov ax, strict word 0000ch                ; b8 0c 00
    jmp near 03c4dh                           ; e9 86 01
    mov ax, word [bp-007feh]                  ; 8b 86 02 f8
    mov dx, word [bp-007fch]                  ; 8b 96 04 f8
    mov word [es:si+008h], ax                 ; 26 89 44 08
    mov word [es:si+00ah], dx                 ; 26 89 54 0a
    mov word [bp-026h], strict word 00028h    ; c7 46 da 28 00
    xchg ah, al                               ; 86 c4
    xchg dh, dl                               ; 86 d6
    xchg dx, ax                               ; 92
    mov word [bp-024h], ax                    ; 89 46 dc
    mov word [bp-022h], dx                    ; 89 56 de
    lea dx, [di-001h]                         ; 8d 55 ff
    shr dx, 002h                              ; c1 ea 02
    inc dx                                    ; 42
    mov ax, dx                                ; 89 d0
    xchg ah, al                               ; 86 c4
    mov word [bp-01fh], ax                    ; 89 46 e1
    les bx, [bp-014h]                         ; c4 5e ec
    mov word [es:bx+00ah], dx                 ; 26 89 57 0a
    mov word [es:bx+00ch], 00200h             ; 26 c7 47 0c 00 02
    mov ax, di                                ; 89 f8
    sal ax, 009h                              ; c1 e0 09
    mov dx, 00800h                            ; ba 00 08
    sub dx, ax                                ; 29 c2
    mov ax, dx                                ; 89 d0
    and ah, 007h                              ; 80 e4 07
    mov word [es:bx+01ch], ax                 ; 26 89 47 1c
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    add bx, ax                                ; 01 c3
    movzx ax, byte [es:bx+01eh]               ; 26 0f b6 47 1e
    add ax, ax                                ; 01 c0
    mov word [bp-01ah], ax                    ; 89 46 e6
    push word [bp-016h]                       ; ff 76 ea
    push dword 000000001h                     ; 66 6a 01
    mov ax, di                                ; 89 f8
    xor di, di                                ; 31 ff
    mov cx, strict word 00009h                ; b9 09 00
    sal ax, 1                                 ; d1 e0
    rcl di, 1                                 ; d1 d7
    loop 03b35h                               ; e2 fa
    push di                                   ; 57
    push ax                                   ; 50
    push strict byte 00000h                   ; 6a 00
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4
    mov cx, ss                                ; 8c d1
    lea bx, [bp-026h]                         ; 8d 5e da
    mov dx, strict word 0000ch                ; ba 0c 00
    mov di, word [bp-01ah]                    ; 8b 7e e6
    call word [word di+0006ah]                ; ff 95 6a 00
    les bx, [bp-014h]                         ; c4 5e ec
    mov word [es:bx+01ch], strict word 00000h ; 26 c7 47 1c 00 00
    test ax, ax                               ; 85 c0
    je short 03b65h                           ; 74 06
    mov ax, strict word 0000dh                ; b8 0d 00
    jmp near 03c4dh                           ; e9 e8 00
    mov es, [bp-010h]                         ; 8e 46 f0
    mov al, byte [es:si+001h]                 ; 26 8a 44 01
    cmp AL, strict byte 002h                  ; 3c 02
    jc short 03b7dh                           ; 72 0d
    jbe short 03b95h                          ; 76 23
    cmp AL, strict byte 004h                  ; 3c 04
    je short 03babh                           ; 74 35
    cmp AL, strict byte 003h                  ; 3c 03
    je short 03ba0h                           ; 74 26
    jmp near 03bf6h                           ; e9 79 00
    cmp AL, strict byte 001h                  ; 3c 01
    jne short 03bf6h                          ; 75 75
    mov es, [bp-010h]                         ; 8e 46 f0
    db  066h, 026h, 0c7h, 044h, 012h, 050h, 000h, 00fh, 000h
    ; mov dword [es:si+012h], strict dword 0000f0050h ; 66 26 c7 44 12 50 00 0f 00
    mov word [es:si+010h], strict word 00002h ; 26 c7 44 10 02 00
    jmp short 03bf6h                          ; eb 61
    db  066h, 026h, 0c7h, 044h, 012h, 050h, 000h, 012h, 000h
    ; mov dword [es:si+012h], strict dword 000120050h ; 66 26 c7 44 12 50 00 12 00
    jmp short 03b8dh                          ; eb ed
    db  066h, 026h, 0c7h, 044h, 012h, 050h, 000h, 024h, 000h
    ; mov dword [es:si+012h], strict dword 000240050h ; 66 26 c7 44 12 50 00 24 00
    jmp short 03b8dh                          ; eb e2
    mov dx, 001c4h                            ; ba c4 01
    mov ax, word [bp-016h]                    ; 8b 46 ea
    call 01650h                               ; e8 9c da
    and AL, strict byte 03fh                  ; 24 3f
    xor ah, ah                                ; 30 e4
    mov es, [bp-010h]                         ; 8e 46 f0
    mov word [es:si+014h], ax                 ; 26 89 44 14
    mov dx, 001c4h                            ; ba c4 01
    mov ax, word [bp-016h]                    ; 8b 46 ea
    call 01650h                               ; e8 88 da
    movzx bx, al                              ; 0f b6 d8
    sal bx, 002h                              ; c1 e3 02
    mov dx, 001c5h                            ; ba c5 01
    mov ax, word [bp-016h]                    ; 8b 46 ea
    call 01650h                               ; e8 79 da
    xor ah, ah                                ; 30 e4
    add ax, bx                                ; 01 d8
    inc ax                                    ; 40
    mov es, [bp-010h]                         ; 8e 46 f0
    mov word [es:si+012h], ax                 ; 26 89 44 12
    mov dx, 001c3h                            ; ba c3 01
    mov ax, word [bp-016h]                    ; 8b 46 ea
    call 01650h                               ; e8 64 da
    xor ah, ah                                ; 30 e4
    inc ax                                    ; 40
    mov es, [bp-010h]                         ; 8e 46 f0
    mov word [es:si+010h], ax                 ; 26 89 44 10
    mov es, [bp-010h]                         ; 8e 46 f0
    cmp byte [es:si+001h], 000h               ; 26 80 7c 01 00
    je short 03c34h                           ; 74 34
    cmp byte [es:si+002h], 000h               ; 26 80 7c 02 00
    jne short 03c1dh                          ; 75 16
    mov dx, strict word 00010h                ; ba 10 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01650h                               ; e8 40 da
    or AL, strict byte 041h                   ; 0c 41
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 00010h                ; ba 10 00
    mov ax, strict word 00040h                ; b8 40 00
    jmp short 03c31h                          ; eb 14
    mov dx, 002c0h                            ; ba c0 02
    mov ax, word [bp-018h]                    ; 8b 46 e8
    call 01650h                               ; e8 2a da
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    movzx bx, al                              ; 0f b6 d8
    mov dx, 002c0h                            ; ba c0 02
    mov ax, word [bp-018h]                    ; 8b 46 e8
    call 0165eh                               ; e8 2a da
    mov es, [bp-010h]                         ; 8e 46 f0
    cmp byte [es:si+001h], 000h               ; 26 80 7c 01 00
    je short 03c42h                           ; 74 04
    mov byte [es:si], 001h                    ; 26 c6 04 01
    mov es, [bp-010h]                         ; 8e 46 f0
    movzx ax, byte [es:si+002h]               ; 26 0f b6 44 02
    sal ax, 008h                              ; c1 e0 08
    lea sp, [bp-00ah]                         ; 8d 66 f6
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
    db  050h, 04eh, 049h, 048h, 047h, 046h, 045h, 044h, 043h, 042h, 041h, 018h, 016h, 015h, 014h, 011h
    db  010h, 00dh, 00ch, 00bh, 00ah, 009h, 008h, 005h, 004h, 003h, 002h, 001h, 000h, 048h, 040h, 056h
    db  03dh, 0a0h, 03dh, 0c8h, 03dh, 095h, 03dh, 0c8h, 03dh, 095h, 03dh, 09eh, 03fh, 07bh, 03dh, 048h
    db  040h, 048h, 040h, 07bh, 03dh, 07bh, 03dh, 07bh, 03dh, 07bh, 03dh, 07bh, 03dh, 03fh, 040h, 07bh
    db  03dh, 048h, 040h, 048h, 040h, 048h, 040h, 048h, 040h, 048h, 040h, 048h, 040h, 048h, 040h, 048h
    db  040h, 048h, 040h, 048h, 040h, 048h, 040h, 048h, 040h
_int13_cdemu:                                ; 0xf3cb0 LB 0x434
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    push di                                   ; 57
    sub sp, strict byte 0002ah                ; 83 ec 2a
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 ab d9
    mov di, 00322h                            ; bf 22 03
    mov cx, ax                                ; 89 c1
    mov si, di                                ; 89 fe
    mov word [bp-008h], ax                    ; 89 46 f8
    mov word [bp-00eh], 00122h                ; c7 46 f2 22 01
    mov word [bp-00ch], ax                    ; 89 46 f4
    mov es, ax                                ; 8e c0
    mov al, byte [es:di+003h]                 ; 26 8a 45 03
    add al, al                                ; 00 c0
    mov byte [bp-006h], al                    ; 88 46 fa
    mov al, byte [es:di+004h]                 ; 26 8a 45 04
    add byte [bp-006h], al                    ; 00 46 fa
    xor bx, bx                                ; 31 db
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 6e d9
    mov es, cx                                ; 8e c1
    cmp byte [es:di], 000h                    ; 26 80 3d 00
    je short 03d06h                           ; 74 0e
    movzx dx, byte [es:di+002h]               ; 26 0f b6 55 02
    mov ax, word [bp+012h]                    ; 8b 46 12
    xor ah, ah                                ; 30 e4
    cmp dx, ax                                ; 39 c2
    je short 03d2fh                           ; 74 29
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 20 dc
    mov ax, word [bp+012h]                    ; 8b 46 12
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 0032ch                               ; 68 2c 03
    push 00338h                               ; 68 38 03
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 49 dc
    add sp, strict byte 0000ah                ; 83 c4 0a
    jmp near 04068h                           ; e9 39 03
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    cmp ax, strict word 00050h                ; 3d 50 00
    jnbe near 04048h                          ; 0f 87 0c 03
    push CS                                   ; 0e
    pop ES                                    ; 07
    mov cx, strict word 0001eh                ; b9 1e 00
    mov di, 03c57h                            ; bf 57 3c
    repne scasb                               ; f2 ae
    sal cx, 1                                 ; d1 e1
    mov di, cx                                ; 89 cf
    mov ax, word [cs:di+03c74h]               ; 2e 8b 85 74 3c
    mov bx, word [bp+016h]                    ; 8b 5e 16
    xor bh, bh                                ; 30 ff
    jmp ax                                    ; ff e0
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa
    imul bx, bx, strict byte 00018h           ; 6b db 18
    mov es, [bp-00ch]                         ; 8e 46 f4
    add bx, word [bp-00eh]                    ; 03 5e f2
    movzx bx, byte [es:bx+01eh]               ; 26 0f b6 5f 1e
    add bx, bx                                ; 01 db
    cmp word [word bx+0006ah], strict byte 00000h ; 83 bf 6a 00 00
    je near 03d7bh                            ; 0f 84 08 00
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa
    call word [word bx+00076h]                ; ff 97 76 00
    mov byte [bp+017h], 000h                  ; c6 46 17 00
    xor bx, bx                                ; 31 db
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 d4 d8
    and byte [bp+01ch], 0feh                  ; 80 66 1c fe
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn                                      ; c3
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    or ah, 003h                               ; 80 cc 03
    jmp near 04070h                           ; e9 d0 02
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01650h                               ; e8 a7 d8
    mov cl, al                                ; 88 c1
    movzx ax, cl                              ; 0f b6 c1
    sal ax, 008h                              ; c1 e0 08
    or bx, ax                                 ; 09 c3
    mov word [bp+016h], bx                    ; 89 5e 16
    xor bx, bx                                ; 31 db
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 9d d8
    test cl, cl                               ; 84 c9
    je short 03d7fh                           ; 74 ba
    jmp near 04084h                           ; e9 bc 02
    mov es, [bp-008h]                         ; 8e 46 f8
    mov di, word [es:si+014h]                 ; 26 8b 7c 14
    mov dx, word [es:si+012h]                 ; 26 8b 54 12
    mov bx, word [es:si+010h]                 ; 26 8b 5c 10
    mov ax, word [es:si+008h]                 ; 26 8b 44 08
    mov word [bp-014h], ax                    ; 89 46 ec
    mov ax, word [es:si+00ah]                 ; 26 8b 44 0a
    mov word [bp-012h], ax                    ; 89 46 ee
    mov ax, word [bp+014h]                    ; 8b 46 14
    and ax, strict word 0003fh                ; 25 3f 00
    mov word [bp-010h], ax                    ; 89 46 f0
    mov cx, word [bp+014h]                    ; 8b 4e 14
    and cx, 000c0h                            ; 81 e1 c0 00
    sal cx, 002h                              ; c1 e1 02
    mov ax, word [bp+014h]                    ; 8b 46 14
    shr ax, 008h                              ; c1 e8 08
    or ax, cx                                 ; 09 c8
    mov si, word [bp+012h]                    ; 8b 76 12
    shr si, 008h                              ; c1 ee 08
    mov cx, word [bp+016h]                    ; 8b 4e 16
    xor ch, ch                                ; 30 ed
    mov word [bp-00ah], cx                    ; 89 4e f6
    test cx, cx                               ; 85 c9
    jne short 03e15h                          ; 75 03
    jmp near 03d7bh                           ; e9 66 ff
    cmp di, word [bp-010h]                    ; 3b 7e f0
    jc near 04068h                            ; 0f 82 4c 02
    cmp ax, dx                                ; 39 d0
    jnc near 04068h                           ; 0f 83 46 02
    cmp si, bx                                ; 39 de
    jnc near 04068h                           ; 0f 83 40 02
    mov dx, word [bp+016h]                    ; 8b 56 16
    shr dx, 008h                              ; c1 ea 08
    cmp dx, strict byte 00004h                ; 83 fa 04
    jne short 03e36h                          ; 75 03
    jmp near 03d7bh                           ; e9 45 ff
    mov dx, word [bp+010h]                    ; 8b 56 10
    shr dx, 004h                              ; c1 ea 04
    mov cx, word [bp+006h]                    ; 8b 4e 06
    add cx, dx                                ; 01 d1
    mov word [bp-016h], cx                    ; 89 4e ea
    mov dx, word [bp+010h]                    ; 8b 56 10
    and dx, strict byte 0000fh                ; 83 e2 0f
    mov word [bp-01ch], dx                    ; 89 56 e4
    xor dl, dl                                ; 30 d2
    xor cx, cx                                ; 31 c9
    call 094a9h                               ; e8 55 56
    xor bx, bx                                ; 31 db
    add ax, si                                ; 01 f0
    adc dx, bx                                ; 11 da
    mov bx, di                                ; 89 fb
    xor cx, cx                                ; 31 c9
    call 094a9h                               ; e8 48 56
    mov bx, ax                                ; 89 c3
    mov ax, word [bp-010h]                    ; 8b 46 f0
    dec ax                                    ; 48
    xor cx, cx                                ; 31 c9
    add ax, bx                                ; 01 d8
    adc dx, cx                                ; 11 ca
    mov bx, word [bp+016h]                    ; 8b 5e 16
    xor bl, bl                                ; 30 db
    mov cx, word [bp-00ah]                    ; 8b 4e f6
    or cx, bx                                 ; 09 d9
    mov word [bp+016h], cx                    ; 89 4e 16
    mov si, ax                                ; 89 c6
    mov di, dx                                ; 89 d7
    shr di, 1                                 ; d1 ef
    rcr si, 1                                 ; d1 de
    shr di, 1                                 ; d1 ef
    rcr si, 1                                 ; d1 de
    mov word [bp-01eh], di                    ; 89 7e e2
    mov di, ax                                ; 89 c7
    and di, strict byte 00003h                ; 83 e7 03
    xor bh, bh                                ; 30 ff
    add ax, word [bp-00ah]                    ; 03 46 f6
    adc dx, bx                                ; 11 da
    add ax, strict word 0ffffh                ; 05 ff ff
    adc dx, strict byte 0ffffh                ; 83 d2 ff
    mov word [bp-022h], ax                    ; 89 46 de
    mov word [bp-020h], dx                    ; 89 56 e0
    shr word [bp-020h], 1                     ; d1 6e e0
    rcr word [bp-022h], 1                     ; d1 5e de
    shr word [bp-020h], 1                     ; d1 6e e0
    rcr word [bp-022h], 1                     ; d1 5e de
    mov cx, strict word 0000ch                ; b9 0c 00
    mov dx, ss                                ; 8c d2
    lea ax, [bp-02eh]                         ; 8d 46 d2
    call 094dah                               ; e8 22 56
    mov word [bp-02eh], strict word 00028h    ; c7 46 d2 28 00
    mov ax, word [bp-014h]                    ; 8b 46 ec
    add ax, si                                ; 01 f0
    mov dx, word [bp-012h]                    ; 8b 56 ee
    adc dx, word [bp-01eh]                    ; 13 56 e2
    xchg ah, al                               ; 86 c4
    xchg dh, dl                               ; 86 d6
    xchg dx, ax                               ; 92
    mov word [bp-02ch], ax                    ; 89 46 d4
    mov word [bp-02ah], dx                    ; 89 56 d6
    mov ax, word [bp-022h]                    ; 8b 46 de
    sub ax, si                                ; 29 f0
    inc ax                                    ; 40
    xchg ah, al                               ; 86 c4
    mov word [bp-027h], ax                    ; 89 46 d9
    mov ax, word [bp-00ah]                    ; 8b 46 f6
    les bx, [bp-00eh]                         ; c4 5e f2
    mov word [es:bx+00ah], ax                 ; 26 89 47 0a
    mov word [es:bx+00ch], 00200h             ; 26 c7 47 0c 00 02
    mov ax, di                                ; 89 f8
    sal ax, 009h                              ; c1 e0 09
    mov word [es:bx+01ah], ax                 ; 26 89 47 1a
    mov dx, word [bp-00ah]                    ; 8b 56 f6
    xor dh, dh                                ; 30 f6
    and dl, 003h                              ; 80 e2 03
    mov bx, strict word 00004h                ; bb 04 00
    sub bx, dx                                ; 29 d3
    mov dx, bx                                ; 89 da
    sub dx, di                                ; 29 fa
    sal dx, 009h                              ; c1 e2 09
    and dh, 007h                              ; 80 e6 07
    mov bx, word [bp-00eh]                    ; 8b 5e f2
    mov word [es:bx+01ch], dx                 ; 26 89 57 1c
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa
    imul dx, dx, strict byte 00018h           ; 6b d2 18
    add bx, dx                                ; 01 d3
    movzx dx, byte [es:bx+01eh]               ; 26 0f b6 57 1e
    add dx, dx                                ; 01 d2
    mov word [bp-01ah], dx                    ; 89 56 e6
    push word [bp-016h]                       ; ff 76 ea
    push word [bp-01ch]                       ; ff 76 e4
    push strict byte 00001h                   ; 6a 01
    mov si, word [bp-00ah]                    ; 8b 76 f6
    xor di, di                                ; 31 ff
    mov cx, strict word 00009h                ; b9 09 00
    sal si, 1                                 ; d1 e6
    rcl di, 1                                 ; d1 d7
    loop 03f38h                               ; e2 fa
    push di                                   ; 57
    push si                                   ; 56
    push ax                                   ; 50
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa
    mov cx, ss                                ; 8c d1
    lea bx, [bp-02eh]                         ; 8d 5e d2
    mov dx, strict word 0000ch                ; ba 0c 00
    mov si, word [bp-01ah]                    ; 8b 76 e6
    call word [word si+0006ah]                ; ff 94 6a 00
    mov dx, ax                                ; 89 c2
    les bx, [bp-00eh]                         ; c4 5e f2
    db  066h, 026h, 0c7h, 047h, 01ah, 000h, 000h, 000h, 000h
    ; mov dword [es:bx+01ah], strict dword 000000000h ; 66 26 c7 47 1a 00 00 00 00
    test al, al                               ; 84 c0
    je near 03d7bh                            ; 0f 84 13 fe
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 be d9
    movzx ax, dl                              ; 0f b6 c2
    push ax                                   ; 50
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 0032ch                               ; 68 2c 03
    push 0036eh                               ; 68 6e 03
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 e9 d9
    add sp, strict byte 0000ah                ; 83 c4 0a
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    or ah, 002h                               ; 80 cc 02
    mov word [bp+016h], ax                    ; 89 46 16
    mov byte [bp+016h], 000h                  ; c6 46 16 00
    jmp near 04073h                           ; e9 d5 00
    mov es, [bp-008h]                         ; 8e 46 f8
    mov di, word [es:si+014h]                 ; 26 8b 7c 14
    mov dx, word [es:si+012h]                 ; 26 8b 54 12
    dec dx                                    ; 4a
    mov bx, word [es:si+010h]                 ; 26 8b 5c 10
    dec bx                                    ; 4b
    mov byte [bp+016h], 000h                  ; c6 46 16 00
    mov ax, word [bp+010h]                    ; 8b 46 10
    xor al, al                                ; 30 c0
    mov cx, word [bp+014h]                    ; 8b 4e 14
    xor ch, ch                                ; 30 ed
    mov word [bp-01ah], cx                    ; 89 4e e6
    mov cx, dx                                ; 89 d1
    xor ch, dh                                ; 30 f5
    sal cx, 008h                              ; c1 e1 08
    mov word [bp-018h], cx                    ; 89 4e e8
    mov cx, word [bp-01ah]                    ; 8b 4e e6
    or cx, word [bp-018h]                     ; 0b 4e e8
    mov word [bp+014h], cx                    ; 89 4e 14
    shr dx, 002h                              ; c1 ea 02
    xor dh, dh                                ; 30 f6
    and dl, 0c0h                              ; 80 e2 c0
    and di, strict byte 0003fh                ; 83 e7 3f
    or dx, di                                 ; 09 fa
    xor cl, cl                                ; 30 c9
    or cx, dx                                 ; 09 d1
    mov word [bp+014h], cx                    ; 89 4e 14
    mov dx, word [bp+012h]                    ; 8b 56 12
    xor dh, dh                                ; 30 f6
    sal bx, 008h                              ; c1 e3 08
    or dx, bx                                 ; 09 da
    mov word [bp+012h], dx                    ; 89 56 12
    xor dl, dl                                ; 30 d2
    or dl, 002h                               ; 80 ca 02
    mov word [bp+012h], dx                    ; 89 56 12
    mov dl, byte [es:si+001h]                 ; 26 8a 54 01
    mov word [bp+010h], ax                    ; 89 46 10
    cmp dl, 003h                              ; 80 fa 03
    je short 04022h                           ; 74 1a
    cmp dl, 002h                              ; 80 fa 02
    je short 0401eh                           ; 74 11
    cmp dl, 001h                              ; 80 fa 01
    jne short 04026h                          ; 75 14
    mov ax, word [bp+010h]                    ; 8b 46 10
    xor al, al                                ; 30 c0
    or AL, strict byte 002h                   ; 0c 02
    mov word [bp+010h], ax                    ; 89 46 10
    jmp short 04026h                          ; eb 08
    or AL, strict byte 004h                   ; 0c 04
    jmp short 04019h                          ; eb f7
    or AL, strict byte 005h                   ; 0c 05
    jmp short 04019h                          ; eb f3
    mov es, [bp-008h]                         ; 8e 46 f8
    cmp byte [es:si+001h], 004h               ; 26 80 7c 01 04
    jnc near 03d7bh                           ; 0f 83 49 fd
    mov word [bp+008h], 0efc7h                ; c7 46 08 c7 ef
    mov word [bp+006h], 0f000h                ; c7 46 06 00 f0
    jmp near 03d7bh                           ; e9 3c fd
    or bh, 003h                               ; 80 cf 03
    mov word [bp+016h], bx                    ; 89 5e 16
    jmp near 03d7fh                           ; e9 37 fd
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 de d8
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 0032ch                               ; 68 2c 03
    push 0038fh                               ; 68 8f 03
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 0d d9
    add sp, strict byte 00008h                ; 83 c4 08
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    or ah, 001h                               ; 80 cc 01
    mov word [bp+016h], ax                    ; 89 46 16
    mov bx, word [bp+016h]                    ; 8b 5e 16
    shr bx, 008h                              ; c1 eb 08
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 da d5
    or byte [bp+01ch], 001h                   ; 80 4e 1c 01
    jmp near 03d8eh                           ; e9 03 fd
    db  050h, 04eh, 049h, 048h, 047h, 046h, 045h, 044h, 043h, 042h, 041h, 018h, 016h, 015h, 014h, 011h
    db  010h, 00dh, 00ch, 00bh, 00ah, 009h, 008h, 005h, 004h, 003h, 002h, 001h, 000h, 029h, 046h, 0a5h
    db  043h, 091h, 041h, 029h, 046h, 086h, 041h, 029h, 046h, 086h, 041h, 029h, 046h, 0a5h, 043h, 029h
    db  046h, 029h, 046h, 0a5h, 043h, 0a5h, 043h, 0a5h, 043h, 0a5h, 043h, 0a5h, 043h, 0bbh, 041h, 0a5h
    db  043h, 029h, 046h, 0c4h, 041h, 0d7h, 041h, 086h, 041h, 0d7h, 041h, 005h, 043h, 0bfh, 043h, 0d7h
    db  041h, 0e6h, 043h, 0e2h, 045h, 0eah, 045h, 029h, 046h
_int13_cdrom:                                ; 0xf40e4 LB 0x562
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    push di                                   ; 57
    sub sp, strict byte 00028h                ; 83 ec 28
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 77 d5
    mov word [bp-01ah], ax                    ; 89 46 e6
    mov si, 00122h                            ; be 22 01
    mov word [bp-00ch], ax                    ; 89 46 f4
    xor bx, bx                                ; 31 db
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 55 d5
    mov ax, word [bp+010h]                    ; 8b 46 10
    xor ah, ah                                ; 30 e4
    cmp ax, 000e0h                            ; 3d e0 00
    jc short 04118h                           ; 72 05
    cmp ax, 000f0h                            ; 3d f0 00
    jc short 04136h                           ; 72 1e
    mov ax, word [bp+010h]                    ; 8b 46 10
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, word [bp+018h]                    ; 8b 46 18
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 003bfh                               ; 68 bf 03
    push 003cbh                               ; 68 cb 03
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 42 d8
    add sp, strict byte 0000ah                ; 83 c4 0a
    jmp near 04606h                           ; e9 d0 04
    mov ax, word [bp+010h]                    ; 8b 46 10
    xor ah, ah                                ; 30 e4
    mov es, [bp-00ch]                         ; 8e 46 f4
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    mov dl, byte [es:bx+000d0h]               ; 26 8a 97 d0 00
    mov byte [bp-008h], dl                    ; 88 56 f8
    cmp dl, 010h                              ; 80 fa 10
    jc short 0415fh                           ; 72 10
    push ax                                   ; 50
    mov ax, word [bp+018h]                    ; 8b 46 18
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 003bfh                               ; 68 bf 03
    push 003f6h                               ; 68 f6 03
    jmp short 0412bh                          ; eb cc
    mov ax, word [bp+018h]                    ; 8b 46 18
    shr ax, 008h                              ; c1 e8 08
    cmp ax, strict word 00050h                ; 3d 50 00
    jnbe near 04629h                          ; 0f 87 bd 04
    push CS                                   ; 0e
    pop ES                                    ; 07
    mov cx, strict word 0001eh                ; b9 1e 00
    mov di, 0408bh                            ; bf 8b 40
    repne scasb                               ; f2 ae
    sal cx, 1                                 ; d1 e1
    mov di, cx                                ; 89 cf
    mov ax, word [cs:di+040a8h]               ; 2e 8b 85 a8 40
    mov bx, word [bp+018h]                    ; 8b 5e 18
    xor bh, bh                                ; 30 ff
    jmp ax                                    ; ff e0
    mov ax, word [bp+018h]                    ; 8b 46 18
    xor ah, ah                                ; 30 e4
    or ah, 003h                               ; 80 cc 03
    jmp near 0460eh                           ; e9 7d 04
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01650h                               ; e8 b6 d4
    mov cl, al                                ; 88 c1
    movzx ax, cl                              ; 0f b6 c1
    sal ax, 008h                              ; c1 e0 08
    or bx, ax                                 ; 09 c3
    mov word [bp+018h], bx                    ; 89 5e 18
    xor bx, bx                                ; 31 db
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 ac d4
    test cl, cl                               ; 84 c9
    je near 043a9h                            ; 0f 84 f1 01
    jmp near 04622h                           ; e9 67 04
    or bh, 002h                               ; 80 cf 02
    mov word [bp+018h], bx                    ; 89 5e 18
    jmp near 04611h                           ; e9 4d 04
    mov word [bp+012h], 0aa55h                ; c7 46 12 55 aa
    or bh, 030h                               ; 80 cf 30
    mov word [bp+018h], bx                    ; 89 5e 18
    mov word [bp+016h], strict word 00007h    ; c7 46 16 07 00
    jmp near 043a9h                           ; e9 d2 01
    mov bx, word [bp+00ch]                    ; 8b 5e 0c
    mov es, [bp+006h]                         ; 8e 46 06
    mov word [bp-014h], bx                    ; 89 5e ec
    mov [bp-012h], es                         ; 8c 46 ee
    mov ax, word [es:bx+002h]                 ; 26 8b 47 02
    mov word [bp-010h], ax                    ; 89 46 f0
    mov ax, word [es:bx+006h]                 ; 26 8b 47 06
    mov word [bp-01ch], ax                    ; 89 46 e4
    mov ax, word [es:bx+004h]                 ; 26 8b 47 04
    mov word [bp-020h], ax                    ; 89 46 e0
    mov ax, word [es:bx+00ch]                 ; 26 8b 47 0c
    mov word [bp-018h], ax                    ; 89 46 e8
    mov di, word [es:bx+00eh]                 ; 26 8b 7f 0e
    or di, ax                                 ; 09 c7
    je short 0421fh                           ; 74 18
    mov ax, word [bp+018h]                    ; 8b 46 18
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 003bfh                               ; 68 bf 03
    push 00428h                               ; 68 28 04
    push strict byte 00007h                   ; 6a 07
    call 01972h                               ; e8 59 d7
    add sp, strict byte 00008h                ; 83 c4 08
    jmp near 04606h                           ; e9 e7 03
    les bx, [bp-014h]                         ; c4 5e ec
    mov ax, word [es:bx+008h]                 ; 26 8b 47 08
    mov word [bp-018h], ax                    ; 89 46 e8
    mov di, bx                                ; 89 df
    mov di, word [es:di+00ah]                 ; 26 8b 7d 0a
    mov ax, word [bp+018h]                    ; 8b 46 18
    shr ax, 008h                              ; c1 e8 08
    mov word [bp-016h], ax                    ; 89 46 ea
    cmp ax, strict word 00044h                ; 3d 44 00
    je near 043a5h                            ; 0f 84 66 01
    cmp ax, strict word 00047h                ; 3d 47 00
    je near 043a5h                            ; 0f 84 5f 01
    mov cx, strict word 0000ch                ; b9 0c 00
    xor bx, bx                                ; 31 db
    mov dx, ss                                ; 8c d2
    lea ax, [bp-02ch]                         ; 8d 46 d4
    call 094dah                               ; e8 87 52
    mov word [bp-02ch], strict word 00028h    ; c7 46 d4 28 00
    mov ax, word [bp-018h]                    ; 8b 46 e8
    mov dx, di                                ; 89 fa
    xchg ah, al                               ; 86 c4
    xchg dh, dl                               ; 86 d6
    xchg dx, ax                               ; 92
    mov word [bp-02ah], ax                    ; 89 46 d6
    mov word [bp-028h], dx                    ; 89 56 d8
    mov ax, word [bp-010h]                    ; 8b 46 f0
    xchg ah, al                               ; 86 c4
    mov word [bp-025h], ax                    ; 89 46 db
    mov ax, word [bp-010h]                    ; 8b 46 f0
    mov es, [bp-00ch]                         ; 8e 46 f4
    mov word [es:si+00ah], ax                 ; 26 89 44 0a
    mov word [es:si+00ch], 00800h             ; 26 c7 44 0c 00 08
    movzx bx, byte [bp-008h]                  ; 0f b6 5e f8
    imul bx, bx, strict byte 00018h           ; 6b db 18
    add bx, si                                ; 01 f3
    movzx di, byte [es:bx+01eh]               ; 26 0f b6 7f 1e
    add di, di                                ; 01 ff
    push word [bp-01ch]                       ; ff 76 e4
    push word [bp-020h]                       ; ff 76 e0
    push strict byte 00001h                   ; 6a 01
    xor bx, bx                                ; 31 db
    mov cx, strict word 0000bh                ; b9 0b 00
    sal ax, 1                                 ; d1 e0
    rcl bx, 1                                 ; d1 d3
    loop 0429dh                               ; e2 fa
    push bx                                   ; 53
    push ax                                   ; 50
    push strict byte 00000h                   ; 6a 00
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    mov cx, ss                                ; 8c d1
    lea bx, [bp-02ch]                         ; 8d 5e d4
    mov dx, strict word 0000ch                ; ba 0c 00
    call word [word di+0006ah]                ; ff 95 6a 00
    mov dx, ax                                ; 89 c2
    mov es, [bp-00ch]                         ; 8e 46 f4
    mov ax, word [es:si+016h]                 ; 26 8b 44 16
    mov di, word [es:si+018h]                 ; 26 8b 7c 18
    mov cx, strict word 0000bh                ; b9 0b 00
    shr di, 1                                 ; d1 ef
    rcr ax, 1                                 ; d1 d8
    loop 042c7h                               ; e2 fa
    les bx, [bp-014h]                         ; c4 5e ec
    mov word [es:bx+002h], ax                 ; 26 89 47 02
    test dl, dl                               ; 84 d2
    je near 043a5h                            ; 0f 84 cb 00
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 4c d6
    movzx ax, dl                              ; 0f b6 c2
    push ax                                   ; 50
    push word [bp-016h]                       ; ff 76 ea
    push 003bfh                               ; 68 bf 03
    push 00451h                               ; 68 51 04
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 7b d6
    add sp, strict byte 0000ah                ; 83 c4 0a
    mov ax, word [bp+018h]                    ; 8b 46 18
    xor ah, ah                                ; 30 e4
    or ah, 00ch                               ; 80 cc 0c
    jmp near 0460eh                           ; e9 09 03
    cmp bx, strict byte 00002h                ; 83 fb 02
    jnbe near 04606h                          ; 0f 87 fa 02
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    mov es, [bp-00ch]                         ; 8e 46 f4
    mov di, si                                ; 89 f7
    add di, ax                                ; 01 c7
    mov al, byte [es:di+021h]                 ; 26 8a 45 21
    cmp bx, strict byte 00002h                ; 83 fb 02
    je short 04396h                           ; 74 73
    cmp bx, strict byte 00001h                ; 83 fb 01
    je short 04363h                           ; 74 3b
    test bx, bx                               ; 85 db
    jne near 043a5h                           ; 0f 85 77 00
    cmp AL, strict byte 0ffh                  ; 3c ff
    jne short 04344h                          ; 75 12
    mov ax, word [bp+018h]                    ; 8b 46 18
    xor ah, ah                                ; 30 e4
    or ah, 0b4h                               ; 80 cc b4
    mov word [bp+018h], ax                    ; 89 46 18
    xor al, al                                ; 30 c0
    or AL, strict byte 001h                   ; 0c 01
    jmp near 0460eh                           ; e9 ca 02
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8
    imul dx, dx, strict byte 00018h           ; 6b d2 18
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    mov es, [bp-00ch]                         ; 8e 46 f4
    add si, dx                                ; 01 d6
    mov byte [es:si+021h], al                 ; 26 88 44 21
    mov ax, word [bp+018h]                    ; 8b 46 18
    xor al, al                                ; 30 c0
    or AL, strict byte 001h                   ; 0c 01
    mov word [bp+018h], ax                    ; 89 46 18
    jmp near 043a5h                           ; e9 42 00
    test al, al                               ; 84 c0
    jne short 04373h                          ; 75 0c
    or bh, 0b0h                               ; 80 cf b0
    mov word [bp+018h], bx                    ; 89 5e 18
    mov byte [bp+018h], al                    ; 88 46 18
    jmp near 04611h                           ; e9 9e 02
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8
    imul dx, dx, strict byte 00018h           ; 6b d2 18
    db  0feh, 0c8h
    ; dec al                                    ; fe c8
    mov es, [bp-00ch]                         ; 8e 46 f4
    add si, dx                                ; 01 d6
    mov byte [es:si+021h], al                 ; 26 88 44 21
    test al, al                               ; 84 c0
    db  00fh, 095h, 0c0h
    ; setne al                                  ; 0f 95 c0
    movzx dx, al                              ; 0f b6 d0
    mov ax, word [bp+018h]                    ; 8b 46 18
    xor al, al                                ; 30 c0
    or ax, dx                                 ; 09 d0
    jmp short 0435dh                          ; eb c7
    test al, al                               ; 84 c0
    db  00fh, 095h, 0c0h
    ; setne al                                  ; 0f 95 c0
    xor ah, ah                                ; 30 e4
    mov dx, word [bp+018h]                    ; 8b 56 18
    mov dl, al                                ; 88 c2
    mov word [bp+018h], dx                    ; 89 56 18
    mov byte [bp+019h], 000h                  ; c6 46 19 00
    xor bx, bx                                ; 31 db
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 aa d2
    and byte [bp+01eh], 0feh                  ; 80 66 1e fe
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn                                      ; c3
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    mov es, [bp-00ch]                         ; 8e 46 f4
    add si, ax                                ; 01 c6
    mov al, byte [es:si+021h]                 ; 26 8a 44 21
    test al, al                               ; 84 c0
    je short 043d9h                           ; 74 06
    or bh, 0b1h                               ; 80 cf b1
    jmp near 041beh                           ; e9 e5 fd
    je short 043a5h                           ; 74 ca
    mov ax, word [bp+018h]                    ; 8b 46 18
    xor ah, ah                                ; 30 e4
    or ah, 0b1h                               ; 80 cc b1
    jmp near 0460eh                           ; e9 28 02
    mov dx, word [bp+00ch]                    ; 8b 56 0c
    mov cx, word [bp+006h]                    ; 8b 4e 06
    mov bx, dx                                ; 89 d3
    mov word [bp-00ah], cx                    ; 89 4e f6
    mov es, cx                                ; 8e c1
    mov di, dx                                ; 89 d7
    mov ax, word [es:di]                      ; 26 8b 05
    mov word [bp-00eh], ax                    ; 89 46 f2
    cmp ax, strict word 0001ah                ; 3d 1a 00
    jc near 04606h                            ; 0f 82 04 02
    jc short 04453h                           ; 72 4f
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    mov es, [bp-00ch]                         ; 8e 46 f4
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
    cmp word [bp-00eh], strict byte 0001eh    ; 83 7e f2 1e
    jc near 0452ah                            ; 0f 82 cf 00
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov word [es:bx], strict word 0001eh      ; 26 c7 07 1e 00
    mov ax, word [bp-01ah]                    ; 8b 46 e6
    mov word [es:bx+01ch], ax                 ; 26 89 47 1c
    mov word [es:bx+01ah], 00312h             ; 26 c7 47 1a 12 03
    movzx cx, byte [bp-008h]                  ; 0f b6 4e f8
    mov ax, cx                                ; 89 c8
    cwd                                       ; 99
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2
    sar ax, 1                                 ; d1 f8
    movzx di, al                              ; 0f b6 f8
    imul di, di, strict byte 00006h           ; 6b ff 06
    mov es, [bp-00ch]                         ; 8e 46 f4
    add di, si                                ; 01 f7
    mov ax, word [es:di+001c2h]               ; 26 8b 85 c2 01
    mov word [bp-01eh], ax                    ; 89 46 e2
    mov dx, word [es:di+001c4h]               ; 26 8b 95 c4 01
    mov al, byte [es:di+001c1h]               ; 26 8a 85 c1 01
    mov byte [bp-006h], al                    ; 88 46 fa
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
    mov ax, word [bp-01eh]                    ; 8b 46 e2
    mov word [es:si+001f0h], ax               ; 26 89 84 f0 01
    mov word [es:si+001f2h], dx               ; 26 89 94 f2 01
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    cwd                                       ; 99
    mov cx, strict word 00002h                ; b9 02 00
    idiv cx                                   ; f7 f9
    or dl, 00eh                               ; 80 ca 0e
    mov ax, dx                                ; 89 d0
    sal ax, 004h                              ; c1 e0 04
    mov byte [es:si+001f4h], al               ; 26 88 84 f4 01
    mov byte [es:si+001f5h], 0cbh             ; 26 c6 84 f5 01 cb
    mov al, byte [bp-006h]                    ; 8a 46 fa
    mov byte [es:si+001f6h], al               ; 26 88 84 f6 01
    mov word [es:si+001f7h], strict word 00001h ; 26 c7 84 f7 01 01 00
    mov byte [es:si+001f9h], 000h             ; 26 c6 84 f9 01 00
    mov word [es:si+001fah], di               ; 26 89 bc fa 01
    mov word [es:si+001fch], strict word 00000h ; 26 c7 84 fc 01 00 00
    mov byte [es:si+001feh], 011h             ; 26 c6 84 fe 01 11
    xor cl, cl                                ; 30 c9
    xor ch, ch                                ; 30 ed
    jmp short 0450dh                          ; eb 05
    cmp ch, 00fh                              ; 80 fd 0f
    jnc short 04520h                          ; 73 13
    movzx dx, ch                              ; 0f b6 d5
    add dx, 00312h                            ; 81 c2 12 03
    mov ax, word [bp-01ah]                    ; 8b 46 e6
    call 01650h                               ; e8 36 d1
    add cl, al                                ; 00 c1
    db  0feh, 0c5h
    ; inc ch                                    ; fe c5
    jmp short 04508h                          ; eb e8
    neg cl                                    ; f6 d9
    mov es, [bp-00ch]                         ; 8e 46 f4
    mov byte [es:si+001ffh], cl               ; 26 88 8c ff 01
    cmp word [bp-00eh], strict byte 00042h    ; 83 7e f2 42
    jc near 043a5h                            ; 0f 82 73 fe
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    cwd                                       ; 99
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2
    sar ax, 1                                 ; d1 f8
    xor ah, ah                                ; 30 e4
    imul ax, ax, strict byte 00006h           ; 6b c0 06
    mov es, [bp-00ch]                         ; 8e 46 f4
    add si, ax                                ; 01 c6
    mov al, byte [es:si+001c0h]               ; 26 8a 84 c0 01
    mov dx, word [es:si+001c2h]               ; 26 8b 94 c2 01
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov word [es:bx], strict word 00042h      ; 26 c7 07 42 00
    db  066h, 026h, 0c7h, 047h, 01eh, 0ddh, 0beh, 024h, 000h
    ; mov dword [es:bx+01eh], strict dword 00024beddh ; 66 26 c7 47 1e dd be 24 00
    mov word [es:bx+022h], strict word 00000h ; 26 c7 47 22 00 00
    test al, al                               ; 84 c0
    jne short 04573h                          ; 75 09
    db  066h, 026h, 0c7h, 047h, 024h, 049h, 053h, 041h, 020h
    ; mov dword [es:bx+024h], strict dword 020415349h ; 66 26 c7 47 24 49 53 41 20
    mov es, [bp-00ah]                         ; 8e 46 f6
    db  066h, 026h, 0c7h, 047h, 028h, 041h, 054h, 041h, 020h
    ; mov dword [es:bx+028h], strict dword 020415441h ; 66 26 c7 47 28 41 54 41 20
    db  066h, 026h, 0c7h, 047h, 02ch, 020h, 020h, 020h, 020h
    ; mov dword [es:bx+02ch], strict dword 020202020h ; 66 26 c7 47 2c 20 20 20 20
    test al, al                               ; 84 c0
    jne short 0459fh                          ; 75 13
    mov word [es:bx+030h], dx                 ; 26 89 57 30
    db  066h, 026h, 0c7h, 047h, 032h, 000h, 000h, 000h, 000h
    ; mov dword [es:bx+032h], strict dword 000000000h ; 66 26 c7 47 32 00 00 00 00
    mov word [es:bx+036h], strict word 00000h ; 26 c7 47 36 00 00
    mov al, byte [bp-008h]                    ; 8a 46 f8
    and AL, strict byte 001h                  ; 24 01
    xor ah, ah                                ; 30 e4
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov word [es:bx+038h], ax                 ; 26 89 47 38
    db  066h, 026h, 0c7h, 047h, 03ah, 000h, 000h, 000h, 000h
    ; mov dword [es:bx+03ah], strict dword 000000000h ; 66 26 c7 47 3a 00 00 00 00
    mov word [es:bx+03eh], strict word 00000h ; 26 c7 47 3e 00 00
    xor al, al                                ; 30 c0
    mov AH, strict byte 01eh                  ; b4 1e
    jmp short 045c7h                          ; eb 05
    cmp ah, 040h                              ; 80 fc 40
    jnc short 045d6h                          ; 73 0f
    movzx si, ah                              ; 0f b6 f4
    mov es, [bp-00ah]                         ; 8e 46 f6
    add si, bx                                ; 01 de
    add al, byte [es:si]                      ; 26 02 04
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4
    jmp short 045c2h                          ; eb ec
    neg al                                    ; f6 d8
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov byte [es:bx+041h], al                 ; 26 88 47 41
    jmp near 043a5h                           ; e9 c3 fd
    or bh, 006h                               ; 80 cf 06
    mov word [bp+018h], bx                    ; 89 5e 18
    jmp short 04622h                          ; eb 38
    cmp bx, strict byte 00006h                ; 83 fb 06
    je near 043a5h                            ; 0f 84 b4 fd
    cmp bx, strict byte 00001h                ; 83 fb 01
    jc short 04606h                           ; 72 10
    jbe near 043a5h                           ; 0f 86 ab fd
    cmp bx, strict byte 00003h                ; 83 fb 03
    jc short 04606h                           ; 72 07
    cmp bx, strict byte 00004h                ; 83 fb 04
    jbe near 043a5h                           ; 0f 86 9f fd
    mov ax, word [bp+018h]                    ; 8b 46 18
    xor ah, ah                                ; 30 e4
    or ah, 001h                               ; 80 cc 01
    mov word [bp+018h], ax                    ; 89 46 18
    mov bx, word [bp+018h]                    ; 8b 5e 18
    shr bx, 008h                              ; c1 eb 08
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 3c d0
    or byte [bp+01eh], 001h                   ; 80 4e 1e 01
    jmp near 043b8h                           ; e9 8f fd
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 fd d2
    mov ax, word [bp+018h]                    ; 8b 46 18
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 003bfh                               ; 68 bf 03
    push 00313h                               ; 68 13 03
    push strict byte 00004h                   ; 6a 04
    jmp near 04216h                           ; e9 d0 fb
print_boot_device_:                          ; 0xf4646 LB 0x4b
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push cx                                   ; 51
    test al, al                               ; 84 c0
    je short 04653h                           ; 74 05
    mov dx, strict word 00002h                ; ba 02 00
    jmp short 0466dh                          ; eb 1a
    test dl, dl                               ; 84 d2
    je short 0465ch                           ; 74 05
    mov dx, strict word 00003h                ; ba 03 00
    jmp short 0466dh                          ; eb 11
    test bl, 080h                             ; f6 c3 80
    jne short 04665h                          ; 75 04
    xor dh, dh                                ; 30 f6
    jmp short 0466dh                          ; eb 08
    test bl, 080h                             ; f6 c3 80
    je short 0468bh                           ; 74 21
    mov dx, strict word 00001h                ; ba 01 00
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 b9 d2
    imul dx, dx, strict byte 0000ah           ; 6b d2 0a
    add dx, 00db0h                            ; 81 c2 b0 0d
    push dx                                   ; 52
    push 00474h                               ; 68 74 04
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 ea d2
    add sp, strict byte 00006h                ; 83 c4 06
    lea sp, [bp-002h]                         ; 8d 66 fe
    pop cx                                    ; 59
    pop bp                                    ; 5d
    retn                                      ; c3
print_boot_failure_:                         ; 0xf4691 LB 0x93
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    mov dh, cl                                ; 88 ce
    mov ah, bl                                ; 88 dc
    and ah, 07fh                              ; 80 e4 7f
    movzx si, ah                              ; 0f b6 f4
    test al, al                               ; 84 c0
    je short 046beh                           ; 74 1b
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 83 d2
    push 00dc4h                               ; 68 c4 0d
    push 00488h                               ; 68 88 04
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 b9 d2
    add sp, strict byte 00006h                ; 83 c4 06
    jmp short 04702h                          ; eb 44
    test dl, dl                               ; 84 d2
    je short 046d2h                           ; 74 10
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 64 d2
    push 00dceh                               ; 68 ce 0d
    jmp short 046b1h                          ; eb df
    test bl, 080h                             ; f6 c3 80
    je short 046e8h                           ; 74 11
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 4f d2
    push si                                   ; 56
    push 00dbah                               ; 68 ba 0d
    jmp short 046f7h                          ; eb 0f
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 3e d2
    push si                                   ; 56
    push 00db0h                               ; 68 b0 0d
    push 0049dh                               ; 68 9d 04
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 73 d2
    add sp, strict byte 00008h                ; 83 c4 08
    cmp byte [bp+004h], 001h                  ; 80 7e 04 01
    jne short 0471ch                          ; 75 14
    test dh, dh                               ; 84 f6
    jne short 04711h                          ; 75 05
    push 004b5h                               ; 68 b5 04
    jmp short 04714h                          ; eb 03
    push 004dfh                               ; 68 df 04
    push strict byte 00007h                   ; 6a 07
    call 01972h                               ; e8 59 d2
    add sp, strict byte 00004h                ; 83 c4 04
    lea sp, [bp-002h]                         ; 8d 66 fe
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn 00002h                               ; c2 02 00
print_cdromboot_failure_:                    ; 0xf4724 LB 0x27
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    mov dx, ax                                ; 89 c2
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 fa d1
    push dx                                   ; 52
    push 00514h                               ; 68 14 05
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 32 d2
    add sp, strict byte 00006h                ; 83 c4 06
    lea sp, [bp-006h]                         ; 8d 66 fa
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
_int19_function:                             ; 0xf474b LB 0x256
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    push di                                   ; 57
    sub sp, strict byte 0000eh                ; 83 ec 0e
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 10 cf
    mov bx, ax                                ; 89 c3
    mov word [bp-00eh], ax                    ; 89 46 f2
    mov byte [bp-008h], 000h                  ; c6 46 f8 00
    mov ax, strict word 0003dh                ; b8 3d 00
    call 016ach                               ; e8 41 cf
    movzx si, al                              ; 0f b6 f0
    mov ax, strict word 00038h                ; b8 38 00
    call 016ach                               ; e8 38 cf
    and AL, strict byte 0f0h                  ; 24 f0
    xor ah, ah                                ; 30 e4
    sal ax, 004h                              ; c1 e0 04
    or si, ax                                 ; 09 c6
    mov ax, strict word 0003ch                ; b8 3c 00
    call 016ach                               ; e8 29 cf
    and AL, strict byte 00fh                  ; 24 0f
    xor ah, ah                                ; 30 e4
    sal ax, 00ch                              ; c1 e0 0c
    or si, ax                                 ; 09 c6
    mov dx, 00339h                            ; ba 39 03
    mov ax, bx                                ; 89 d8
    call 01650h                               ; e8 bc ce
    test al, al                               ; 84 c0
    je short 047a3h                           ; 74 0b
    mov dx, 00339h                            ; ba 39 03
    mov ax, bx                                ; 89 d8
    call 01650h                               ; e8 b0 ce
    movzx si, al                              ; 0f b6 f0
    cmp byte [bp+004h], 001h                  ; 80 7e 04 01
    jne short 047b9h                          ; 75 10
    mov ax, strict word 0003ch                ; b8 3c 00
    call 016ach                               ; e8 fd ce
    and AL, strict byte 0f0h                  ; 24 f0
    xor ah, ah                                ; 30 e4
    sar ax, 004h                              ; c1 f8 04
    call 075f1h                               ; e8 38 2e
    cmp byte [bp+004h], 002h                  ; 80 7e 04 02
    jne short 047c2h                          ; 75 03
    shr si, 004h                              ; c1 ee 04
    cmp byte [bp+004h], 003h                  ; 80 7e 04 03
    jne short 047cbh                          ; 75 03
    shr si, 008h                              ; c1 ee 08
    cmp byte [bp+004h], 004h                  ; 80 7e 04 04
    jne short 047d4h                          ; 75 03
    shr si, 00ch                              ; c1 ee 0c
    cmp si, strict byte 00010h                ; 83 fe 10
    jnc short 047ddh                          ; 73 04
    mov byte [bp-008h], 001h                  ; c6 46 f8 01
    xor al, al                                ; 30 c0
    mov byte [bp-006h], al                    ; 88 46 fa
    mov byte [bp-00ah], al                    ; 88 46 f6
    mov byte [bp-00ch], al                    ; 88 46 f4
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 3e d1
    push si                                   ; 56
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04
    push ax                                   ; 50
    push 00534h                               ; 68 34 05
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 71 d1
    add sp, strict byte 00008h                ; 83 c4 08
    and si, strict byte 0000fh                ; 83 e6 0f
    cmp si, strict byte 00002h                ; 83 fe 02
    jc short 0481ah                           ; 72 0e
    jbe short 04829h                          ; 76 1b
    cmp si, strict byte 00004h                ; 83 fe 04
    je short 04847h                           ; 74 34
    cmp si, strict byte 00003h                ; 83 fe 03
    je short 0483dh                           ; 74 25
    jmp short 04874h                          ; eb 5a
    cmp si, strict byte 00001h                ; 83 fe 01
    jne short 04874h                          ; 75 55
    xor al, al                                ; 30 c0
    mov byte [bp-006h], al                    ; 88 46 fa
    mov byte [bp-00ah], al                    ; 88 46 f6
    jmp short 0488ch                          ; eb 63
    mov dx, 00338h                            ; ba 38 03
    mov ax, word [bp-00eh]                    ; 8b 46 f2
    call 01650h                               ; e8 1e ce
    add AL, strict byte 080h                  ; 04 80
    mov byte [bp-006h], al                    ; 88 46 fa
    mov byte [bp-00ah], 000h                  ; c6 46 f6 00
    jmp short 0488ch                          ; eb 4f
    mov byte [bp-006h], 000h                  ; c6 46 fa 00
    mov byte [bp-00ah], 001h                  ; c6 46 f6 01
    jmp short 04851h                          ; eb 0a
    mov byte [bp-00ch], 001h                  ; c6 46 f4 01
    cmp byte [bp-00ah], 000h                  ; 80 7e f6 00
    je short 0488ch                           ; 74 3b
    call 0389ah                               ; e8 46 f0
    mov bx, ax                                ; 89 c3
    test AL, strict byte 0ffh                 ; a8 ff
    je short 0487bh                           ; 74 21
    call 04724h                               ; e8 c7 fe
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    push ax                                   ; 50
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa
    movzx dx, byte [bp-00ch]                  ; 0f b6 56 f4
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6
    mov cx, strict word 00001h                ; b9 01 00
    call 04691h                               ; e8 1d fe
    xor ax, ax                                ; 31 c0
    xor dx, dx                                ; 31 d2
    jmp near 0499ah                           ; e9 1f 01
    mov dx, 0032eh                            ; ba 2e 03
    mov ax, word [bp-00eh]                    ; 8b 46 f2
    call 0166ch                               ; e8 e8 cd
    mov di, ax                                ; 89 c7
    shr bx, 008h                              ; c1 eb 08
    mov byte [bp-006h], bl                    ; 88 5e fa
    cmp byte [bp-00ch], 001h                  ; 80 7e f4 01
    jne near 04908h                           ; 0f 85 74 00
    xor si, si                                ; 31 f6
    mov ax, 0e200h                            ; b8 00 e2
    mov es, ax                                ; 8e c0
    cmp word [es:si], 0aa55h                  ; 26 81 3c 55 aa
    jne short 0485dh                          ; 75 bb
    mov cx, ax                                ; 89 c1
    mov si, word [es:si+01ah]                 ; 26 8b 74 1a
    cmp word [es:si+002h], 0506eh             ; 26 81 7c 02 6e 50
    jne short 0485dh                          ; 75 ad
    cmp word [es:si], 05024h                  ; 26 81 3c 24 50
    jne short 0485dh                          ; 75 a6
    mov di, word [es:si+00eh]                 ; 26 8b 7c 0e
    mov dx, word [es:di]                      ; 26 8b 15
    mov ax, word [es:di+002h]                 ; 26 8b 45 02
    cmp ax, 06568h                            ; 3d 68 65
    jne short 048e6h                          ; 75 1f
    cmp dx, 07445h                            ; 81 fa 45 74
    jne short 048e6h                          ; 75 19
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa
    movzx dx, byte [bp-00ch]                  ; 0f b6 56 f4
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6
    call 04646h                               ; e8 6a fd
    mov word [bp-012h], strict word 00006h    ; c7 46 ee 06 00
    mov word [bp-010h], cx                    ; 89 4e f0
    jmp short 04902h                          ; eb 1c
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa
    movzx dx, byte [bp-00ch]                  ; 0f b6 56 f4
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6
    call 04646h                               ; e8 51 fd
    sti                                       ; fb
    mov word [bp-010h], cx                    ; 89 4e f0
    mov es, cx                                ; 8e c1
    mov ax, word [es:si+01ah]                 ; 26 8b 44 1a
    mov word [bp-012h], ax                    ; 89 46 ee
    call far [bp-012h]                        ; ff 5e ee
    jmp near 0485dh                           ; e9 55 ff
    cmp byte [bp-00ah], 000h                  ; 80 7e f6 00
    jne short 04934h                          ; 75 26
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00
    jne short 04934h                          ; 75 20
    mov di, 007c0h                            ; bf c0 07
    mov es, di                                ; 8e c7
    mov dl, byte [bp-006h]                    ; 8a 56 fa
    mov ax, 00201h                            ; b8 01 02
    mov DH, strict byte 000h                  ; b6 00
    mov cx, strict word 00001h                ; b9 01 00
    db  033h, 0dbh
    ; xor bx, bx                                ; 33 db
    int 013h                                  ; cd 13
    mov ax, strict word 00000h                ; b8 00 00
    sbb ax, strict byte 00000h                ; 83 d8 00
    test ax, ax                               ; 85 c0
    jne near 0485dh                           ; 0f 85 29 ff
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00
    db  00fh, 094h, 0c1h
    ; sete cl                                   ; 0f 94 c1
    cmp byte [bp-00ah], 000h                  ; 80 7e f6 00
    je short 04943h                           ; 74 02
    mov CL, strict byte 001h                  ; b1 01
    xor dx, dx                                ; 31 d2
    mov ax, di                                ; 89 f8
    call 0166ch                               ; e8 22 cd
    mov bx, ax                                ; 89 c3
    mov dx, strict word 00002h                ; ba 02 00
    mov ax, di                                ; 89 f8
    call 0166ch                               ; e8 18 cd
    cmp bx, ax                                ; 39 c3
    je short 04969h                           ; 74 11
    test cl, cl                               ; 84 c9
    jne short 0497fh                          ; 75 23
    mov dx, 001feh                            ; ba fe 01
    mov ax, di                                ; 89 f8
    call 0166ch                               ; e8 08 cd
    cmp ax, 0aa55h                            ; 3d 55 aa
    je short 0497fh                           ; 74 16
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    push ax                                   ; 50
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa
    movzx dx, byte [bp-00ch]                  ; 0f b6 56 f4
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6
    xor cx, cx                                ; 31 c9
    jmp near 04871h                           ; e9 f2 fe
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa
    movzx dx, byte [bp-00ch]                  ; 0f b6 56 f4
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6
    call 04646h                               ; e8 b8 fc
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa
    xor dx, dx                                ; 31 d2
    xor ax, ax                                ; 31 c0
    add ax, di                                ; 01 f8
    adc dx, bx                                ; 11 da
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn                                      ; c3
keyboard_panic_:                             ; 0xf49a1 LB 0x13
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push ax                                   ; 50
    push 00554h                               ; 68 54 05
    push strict byte 00007h                   ; 6a 07
    call 01972h                               ; e8 c5 cf
    add sp, strict byte 00006h                ; 83 c4 06
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    retn                                      ; c3
_keyboard_init:                              ; 0xf49b4 LB 0x26a
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
    je short 049d7h                           ; 74 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 049d7h                          ; 76 08
    xor al, al                                ; 30 c0
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 049c0h                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 049e0h                          ; 75 05
    xor ax, ax                                ; 31 c0
    call 049a1h                               ; e8 c1 ff
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 049fah                          ; 75 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 049fah                          ; 76 08
    mov AL, strict byte 001h                  ; b0 01
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 049e3h                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 04a04h                          ; 75 06
    mov ax, strict word 00001h                ; b8 01 00
    call 049a1h                               ; e8 9d ff
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp ax, strict word 00055h                ; 3d 55 00
    je short 04a15h                           ; 74 06
    mov ax, 003dfh                            ; b8 df 03
    call 049a1h                               ; e8 8c ff
    mov AL, strict byte 0abh                  ; b0 ab
    mov dx, strict word 00064h                ; ba 64 00
    out DX, AL                                ; ee
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 002h                 ; a8 02
    je short 04a35h                           ; 74 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 04a35h                          ; 76 08
    mov AL, strict byte 010h                  ; b0 10
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 04a1eh                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 04a3fh                          ; 75 06
    mov ax, strict word 0000ah                ; b8 0a 00
    call 049a1h                               ; e8 62 ff
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 04a59h                          ; 75 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 04a59h                          ; 76 08
    mov AL, strict byte 011h                  ; b0 11
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 04a42h                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 04a63h                          ; 75 06
    mov ax, strict word 0000bh                ; b8 0b 00
    call 049a1h                               ; e8 3e ff
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test ax, ax                               ; 85 c0
    je short 04a73h                           ; 74 06
    mov ax, 003e0h                            ; b8 e0 03
    call 049a1h                               ; e8 2e ff
    mov AL, strict byte 0ffh                  ; b0 ff
    mov dx, strict word 00060h                ; ba 60 00
    out DX, AL                                ; ee
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 002h                 ; a8 02
    je short 04a93h                           ; 74 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 04a93h                          ; 76 08
    mov AL, strict byte 020h                  ; b0 20
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 04a7ch                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 04a9dh                          ; 75 06
    mov ax, strict word 00014h                ; b8 14 00
    call 049a1h                               ; e8 04 ff
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 04ab7h                          ; 75 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 04ab7h                          ; 76 08
    mov AL, strict byte 021h                  ; b0 21
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 04aa0h                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 04ac1h                          ; 75 06
    mov ax, strict word 00015h                ; b8 15 00
    call 049a1h                               ; e8 e0 fe
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp ax, 000fah                            ; 3d fa 00
    je short 04ad2h                           ; 74 06
    mov ax, 003e1h                            ; b8 e1 03
    call 049a1h                               ; e8 cf fe
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 04ae4h                          ; 75 08
    mov AL, strict byte 031h                  ; b0 31
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 04ad2h                          ; eb ee
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp ax, 000aah                            ; 3d aa 00
    je short 04afdh                           ; 74 0e
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp ax, 000aah                            ; 3d aa 00
    je short 04afdh                           ; 74 06
    mov ax, 003e2h                            ; b8 e2 03
    call 049a1h                               ; e8 a4 fe
    mov AL, strict byte 0f5h                  ; b0 f5
    mov dx, strict word 00060h                ; ba 60 00
    out DX, AL                                ; ee
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 002h                 ; a8 02
    je short 04b1dh                           ; 74 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 04b1dh                          ; 76 08
    mov AL, strict byte 040h                  ; b0 40
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 04b06h                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 04b27h                          ; 75 06
    mov ax, strict word 00028h                ; b8 28 00
    call 049a1h                               ; e8 7a fe
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 04b41h                          ; 75 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 04b41h                          ; 76 08
    mov AL, strict byte 041h                  ; b0 41
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 04b2ah                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 04b4bh                          ; 75 06
    mov ax, strict word 00029h                ; b8 29 00
    call 049a1h                               ; e8 56 fe
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp ax, 000fah                            ; 3d fa 00
    je short 04b5ch                           ; 74 06
    mov ax, 003e3h                            ; b8 e3 03
    call 049a1h                               ; e8 45 fe
    mov AL, strict byte 060h                  ; b0 60
    mov dx, strict word 00064h                ; ba 64 00
    out DX, AL                                ; ee
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 002h                 ; a8 02
    je short 04b7ch                           ; 74 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 04b7ch                          ; 76 08
    mov AL, strict byte 050h                  ; b0 50
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 04b65h                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 04b86h                          ; 75 06
    mov ax, strict word 00032h                ; b8 32 00
    call 049a1h                               ; e8 1b fe
    mov AL, strict byte 065h                  ; b0 65
    mov dx, strict word 00060h                ; ba 60 00
    out DX, AL                                ; ee
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 002h                 ; a8 02
    je short 04ba6h                           ; 74 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 04ba6h                          ; 76 08
    mov AL, strict byte 060h                  ; b0 60
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 04b8fh                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 04bb0h                          ; 75 06
    mov ax, strict word 0003ch                ; b8 3c 00
    call 049a1h                               ; e8 f1 fd
    mov AL, strict byte 0f4h                  ; b0 f4
    mov dx, strict word 00060h                ; ba 60 00
    out DX, AL                                ; ee
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 002h                 ; a8 02
    je short 04bd0h                           ; 74 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 04bd0h                          ; 76 08
    mov AL, strict byte 070h                  ; b0 70
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 04bb9h                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 04bdah                          ; 75 06
    mov ax, strict word 00046h                ; b8 46 00
    call 049a1h                               ; e8 c7 fd
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 04bf4h                          ; 75 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 04bf4h                          ; 76 08
    mov AL, strict byte 071h                  ; b0 71
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 04bddh                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 04bfeh                          ; 75 06
    mov ax, strict word 00046h                ; b8 46 00
    call 049a1h                               ; e8 a3 fd
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp ax, 000fah                            ; 3d fa 00
    je short 04c0fh                           ; 74 06
    mov ax, 003e4h                            ; b8 e4 03
    call 049a1h                               ; e8 92 fd
    mov AL, strict byte 0a8h                  ; b0 a8
    mov dx, strict word 00064h                ; ba 64 00
    out DX, AL                                ; ee
    xor ax, ax                                ; 31 c0
    call 05e89h                               ; e8 6f 12
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    retn                                      ; c3
enqueue_key_:                                ; 0xf4c1e LB 0x93
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    push ax                                   ; 50
    mov byte [bp-00ah], al                    ; 88 46 f6
    mov bl, dl                                ; 88 d3
    mov dx, strict word 0001ah                ; ba 1a 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 38 ca
    mov di, ax                                ; 89 c7
    mov dx, strict word 0001ch                ; ba 1c 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 2d ca
    mov si, ax                                ; 89 c6
    lea cx, [si+002h]                         ; 8d 4c 02
    cmp cx, strict byte 0003eh                ; 83 f9 3e
    jc short 04c4ch                           ; 72 03
    mov cx, strict word 0001eh                ; b9 1e 00
    cmp cx, di                                ; 39 f9
    jne short 04c54h                          ; 75 04
    xor ax, ax                                ; 31 c0
    jmp short 04c79h                          ; eb 25
    xor bh, bh                                ; 30 ff
    mov dx, si                                ; 89 f2
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 00 ca
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6
    lea dx, [si+001h]                         ; 8d 54 01
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 f3 c9
    mov bx, cx                                ; 89 cb
    mov dx, strict word 0001ch                ; ba 1c 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0167ah                               ; e8 04 ca
    mov ax, strict word 00001h                ; b8 01 00
    lea sp, [bp-008h]                         ; 8d 66 f8
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    pop bx                                    ; 5b
    pop bp                                    ; 5d
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
    jmp short 04ce1h                          ; eb 4e
    js short 04ce2h                           ; 78 4d
    inc di                                    ; 47
    dec bp                                    ; 4d
    inc di                                    ; 47
    dec bp                                    ; 4d
    cli                                       ; fa
    dec bp                                    ; 4d
    and byte [di+06ch], cl                    ; 20 4d 6c
    dec si                                    ; 4e
    mov bx, 0de4eh                            ; bb 4e de
    dec si                                    ; 4e
    mov cx, 0474dh                            ; b9 4d 47
    dec bp                                    ; 4d
    inc di                                    ; 47
    dec bp                                    ; 4d
    xor cx, word [bp+039h]                    ; 33 4e 39
    dec bp                                    ; 4d
    pushfw                                    ; 9c
    dec si                                    ; 4e
    xlatb                                     ; d7
    dec si                                    ; 4e
_int09_function:                             ; 0xf4cb1 LB 0x35d
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    push di                                   ; 57
    sub sp, strict byte 0000ch                ; 83 ec 0c
    mov al, byte [bp+014h]                    ; 8a 46 14
    mov byte [bp-00ah], al                    ; 88 46 f6
    test al, al                               ; 84 c0
    jne short 04cdch                          ; 75 19
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 63 cc
    push 00567h                               ; 68 67 05
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 9c cc
    add sp, strict byte 00004h                ; 83 c4 04
    jmp near 05007h                           ; e9 2b 03
    mov dx, strict word 00017h                ; ba 17 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01650h                               ; e8 6b c9
    mov byte [bp-00eh], al                    ; 88 46 f2
    mov bl, al                                ; 88 c3
    mov dx, strict word 00018h                ; ba 18 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01650h                               ; e8 5d c9
    mov byte [bp-010h], al                    ; 88 46 f0
    mov byte [bp-00ch], al                    ; 88 46 f4
    mov dx, 00096h                            ; ba 96 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01650h                               ; e8 4e c9
    mov byte [bp-008h], al                    ; 88 46 f8
    mov byte [bp-006h], al                    ; 88 46 fa
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    push CS                                   ; 0e
    pop ES                                    ; 07
    mov cx, strict word 00010h                ; b9 10 00
    mov di, 04c82h                            ; bf 82 4c
    repne scasb                               ; f2 ae
    sal cx, 1                                 ; d1 e1
    mov di, cx                                ; 89 cf
    mov ax, word [cs:di+04c91h]               ; 2e 8b 85 91 4c
    jmp ax                                    ; ff e0
    xor bl, 040h                              ; 80 f3 40
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00017h                ; ba 17 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 30 c9
    or byte [bp-00ch], 040h                   ; 80 4e f4 40
    movzx bx, byte [bp-00ch]                  ; 0f b6 5e f4
    jmp near 04eafh                           ; e9 76 01
    mov al, byte [bp-010h]                    ; 8a 46 f0
    and AL, strict byte 0bfh                  ; 24 bf
    mov byte [bp-00ch], al                    ; 88 46 f4
    movzx bx, al                              ; 0f b6 d8
    jmp near 04eafh                           ; e9 68 01
    test byte [bp-006h], 002h                 ; f6 46 fa 02
    jne near 04fe9h                           ; 0f 85 9a 02
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    and AL, strict byte 07fh                  ; 24 7f
    cmp AL, strict byte 02ah                  ; 3c 2a
    db  00fh, 094h, 0c0h
    ; sete al                                   ; 0f 94 c0
    xor ah, ah                                ; 30 e4
    inc ax                                    ; 40
    test byte [bp-00ah], 080h                 ; f6 46 f6 80
    je short 04d68h                           ; 74 06
    not al                                    ; f6 d0
    and bl, al                                ; 20 c3
    jmp short 04d6ah                          ; eb 02
    or bl, al                                 ; 08 c3
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00017h                ; ba 17 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 e9 c8
    jmp near 04fe9h                           ; e9 71 02
    test byte [bp-008h], 001h                 ; f6 46 f8 01
    jne near 04fe9h                           ; 0f 85 69 02
    or bl, 004h                               ; 80 cb 04
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00017h                ; ba 17 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 d0 c8
    mov al, byte [bp-008h]                    ; 8a 46 f8
    test AL, strict byte 002h                 ; a8 02
    je short 04da2h                           ; 74 0d
    or AL, strict byte 004h                   ; 0c 04
    mov byte [bp-006h], al                    ; 88 46 fa
    movzx bx, al                              ; 0f b6 d8
    mov dx, 00096h                            ; ba 96 00
    jmp short 04db0h                          ; eb 0e
    mov al, byte [bp-010h]                    ; 8a 46 f0
    or AL, strict byte 001h                   ; 0c 01
    mov byte [bp-00ch], al                    ; 88 46 f4
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 00018h                ; ba 18 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 a8 c8
    jmp near 04fe9h                           ; e9 30 02
    test byte [bp-008h], 001h                 ; f6 46 f8 01
    jne near 04fe9h                           ; 0f 85 28 02
    and bl, 0fbh                              ; 80 e3 fb
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00017h                ; ba 17 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 8f c8
    mov al, byte [bp-008h]                    ; 8a 46 f8
    test AL, strict byte 002h                 ; a8 02
    je short 04de3h                           ; 74 0d
    and AL, strict byte 0fbh                  ; 24 fb
    mov byte [bp-006h], al                    ; 88 46 fa
    movzx bx, al                              ; 0f b6 d8
    mov dx, 00096h                            ; ba 96 00
    jmp short 04df1h                          ; eb 0e
    mov al, byte [bp-010h]                    ; 8a 46 f0
    and AL, strict byte 0feh                  ; 24 fe
    mov byte [bp-00ch], al                    ; 88 46 f4
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 00018h                ; ba 18 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 67 c8
    jmp near 04fe9h                           ; e9 ef 01
    or bl, 008h                               ; 80 cb 08
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00017h                ; ba 17 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 56 c8
    mov al, byte [bp-008h]                    ; 8a 46 f8
    test AL, strict byte 002h                 ; a8 02
    je short 04e1ch                           ; 74 0d
    or AL, strict byte 008h                   ; 0c 08
    mov byte [bp-006h], al                    ; 88 46 fa
    movzx bx, al                              ; 0f b6 d8
    mov dx, 00096h                            ; ba 96 00
    jmp short 04e2ah                          ; eb 0e
    mov al, byte [bp-010h]                    ; 8a 46 f0
    or AL, strict byte 002h                   ; 0c 02
    mov byte [bp-00ch], al                    ; 88 46 f4
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 00018h                ; ba 18 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 2e c8
    jmp near 04fe9h                           ; e9 b6 01
    and bl, 0f7h                              ; 80 e3 f7
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00017h                ; ba 17 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 1d c8
    mov al, byte [bp-008h]                    ; 8a 46 f8
    test AL, strict byte 002h                 ; a8 02
    je short 04e55h                           ; 74 0d
    and AL, strict byte 0f7h                  ; 24 f7
    mov byte [bp-006h], al                    ; 88 46 fa
    movzx bx, al                              ; 0f b6 d8
    mov dx, 00096h                            ; ba 96 00
    jmp short 04e63h                          ; eb 0e
    mov al, byte [bp-010h]                    ; 8a 46 f0
    and AL, strict byte 0fdh                  ; 24 fd
    mov byte [bp-00ch], al                    ; 88 46 f4
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 00018h                ; ba 18 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 f5 c7
    jmp near 04fe9h                           ; e9 7d 01
    test byte [bp-008h], 003h                 ; f6 46 f8 03
    jne near 04fe9h                           ; 0f 85 75 01
    mov al, byte [bp-010h]                    ; 8a 46 f0
    or AL, strict byte 020h                   ; 0c 20
    mov byte [bp-00ch], al                    ; 88 46 f4
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 00018h                ; ba 18 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 d6 c7
    mov bl, byte [bp-00eh]                    ; 8a 5e f2
    xor bl, 020h                              ; 80 f3 20
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00017h                ; ba 17 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 c5 c7
    jmp near 04fe9h                           ; e9 4d 01
    test byte [bp-008h], 003h                 ; f6 46 f8 03
    jne near 04fe9h                           ; 0f 85 45 01
    mov al, byte [bp-010h]                    ; 8a 46 f0
    and AL, strict byte 0dfh                  ; 24 df
    mov byte [bp-00ch], al                    ; 88 46 f4
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 00018h                ; ba 18 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 a6 c7
    jmp near 04fe9h                           ; e9 2e 01
    mov al, byte [bp-010h]                    ; 8a 46 f0
    or AL, strict byte 010h                   ; 0c 10
    mov byte [bp-00ch], al                    ; 88 46 f4
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 00018h                ; ba 18 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 8f c7
    mov bl, byte [bp-00eh]                    ; 8a 5e f2
    xor bl, 010h                              ; 80 f3 10
    jmp short 04e8eh                          ; eb b7
    mov al, byte [bp-010h]                    ; 8a 46 f0
    and AL, strict byte 0efh                  ; 24 ef
    jmp short 04ea9h                          ; eb cb
    mov al, bl                                ; 88 d8
    and AL, strict byte 00ch                  ; 24 0c
    cmp AL, strict byte 00ch                  ; 3c 0c
    jne short 04eebh                          ; 75 05
    jmp far 0f000h:0e05bh                     ; ea 5b e0 00 f0
    test byte [bp-00ah], 080h                 ; f6 46 f6 80
    jne near 04fe9h                           ; 0f 85 f6 00
    cmp byte [bp-00ah], 058h                  ; 80 7e f6 58
    jbe short 04f17h                          ; 76 1e
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 2d ca
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6
    push ax                                   ; 50
    push 00581h                               ; 68 81 05
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 61 ca
    add sp, strict byte 00006h                ; 83 c4 06
    jmp near 05007h                           ; e9 f0 00
    test bl, 008h                             ; f6 c3 08
    je short 04f2eh                           ; 74 12
    movzx si, byte [bp-00ah]                  ; 0f b6 76 f6
    imul si, si, strict byte 0000ah           ; 6b f6 0a
    mov dl, byte [si+00ddeh]                  ; 8a 94 de 0d
    mov ax, word [si+00ddeh]                  ; 8b 84 de 0d
    jmp near 04fbah                           ; e9 8c 00
    test bl, 004h                             ; f6 c3 04
    je short 04f45h                           ; 74 12
    movzx si, byte [bp-00ah]                  ; 0f b6 76 f6
    imul si, si, strict byte 0000ah           ; 6b f6 0a
    mov dl, byte [si+00ddch]                  ; 8a 94 dc 0d
    mov ax, word [si+00ddch]                  ; 8b 84 dc 0d
    jmp near 04fbah                           ; e9 75 00
    mov al, byte [bp-006h]                    ; 8a 46 fa
    and AL, strict byte 002h                  ; 24 02
    test al, al                               ; 84 c0
    jbe short 04f63h                          ; 76 15
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    cmp AL, strict byte 047h                  ; 3c 47
    jc short 04f63h                           ; 72 0e
    cmp AL, strict byte 053h                  ; 3c 53
    jnbe short 04f63h                         ; 77 0a
    mov DL, strict byte 0e0h                  ; b2 e0
    movzx si, al                              ; 0f b6 f0
    imul si, si, strict byte 0000ah           ; 6b f6 0a
    jmp short 04fb6h                          ; eb 53
    test bl, 003h                             ; f6 c3 03
    je short 04f95h                           ; 74 2d
    movzx si, byte [bp-00ah]                  ; 0f b6 76 f6
    imul si, si, strict byte 0000ah           ; 6b f6 0a
    movzx ax, byte [si+00de0h]                ; 0f b6 84 e0 0d
    movzx dx, bl                              ; 0f b6 d3
    test dx, ax                               ; 85 c2
    je short 04f85h                           ; 74 0a
    mov dl, byte [si+00dd8h]                  ; 8a 94 d8 0d
    mov ax, word [si+00dd8h]                  ; 8b 84 d8 0d
    jmp short 04f8dh                          ; eb 08
    mov dl, byte [si+00ddah]                  ; 8a 94 da 0d
    mov ax, word [si+00ddah]                  ; 8b 84 da 0d
    shr ax, 008h                              ; c1 e8 08
    mov byte [bp-00ah], al                    ; 88 46 f6
    jmp short 04fc0h                          ; eb 2b
    movzx si, byte [bp-00ah]                  ; 0f b6 76 f6
    imul si, si, strict byte 0000ah           ; 6b f6 0a
    movzx ax, byte [si+00de0h]                ; 0f b6 84 e0 0d
    movzx dx, bl                              ; 0f b6 d3
    test dx, ax                               ; 85 c2
    je short 04fb2h                           ; 74 0a
    mov dl, byte [si+00ddah]                  ; 8a 94 da 0d
    mov ax, word [si+00ddah]                  ; 8b 84 da 0d
    jmp short 04fbah                          ; eb 08
    mov dl, byte [si+00dd8h]                  ; 8a 94 d8 0d
    mov ax, word [si+00dd8h]                  ; 8b 84 d8 0d
    shr ax, 008h                              ; c1 e8 08
    mov byte [bp-00ah], al                    ; 88 46 f6
    cmp byte [bp-00ah], 000h                  ; 80 7e f6 00
    jne short 04fe0h                          ; 75 1a
    test dl, dl                               ; 84 d2
    jne short 04fe0h                          ; 75 16
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 5c c9
    push 005b8h                               ; 68 b8 05
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 95 c9
    add sp, strict byte 00004h                ; 83 c4 04
    xor dh, dh                                ; 30 f6
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6
    call 04c1eh                               ; e8 35 fc
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    and AL, strict byte 07fh                  ; 24 7f
    cmp AL, strict byte 01dh                  ; 3c 1d
    je short 04ff6h                           ; 74 04
    and byte [bp-006h], 0feh                  ; 80 66 fa fe
    and byte [bp-006h], 0fdh                  ; 80 66 fa fd
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa
    mov dx, 00096h                            ; ba 96 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 57 c6
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn                                      ; c3
dequeue_key_:                                ; 0xf500e LB 0x94
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    push di                                   ; 57
    push ax                                   ; 50
    push ax                                   ; 50
    mov di, ax                                ; 89 c7
    mov word [bp-006h], dx                    ; 89 56 fa
    mov si, bx                                ; 89 de
    mov word [bp-008h], cx                    ; 89 4e f8
    mov dx, strict word 0001ah                ; ba 1a 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 44 c6
    mov bx, ax                                ; 89 c3
    mov dx, strict word 0001ch                ; ba 1c 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 39 c6
    cmp bx, ax                                ; 39 c3
    je short 05074h                           ; 74 3d
    mov dx, bx                                ; 89 da
    mov ax, strict word 00040h                ; b8 40 00
    call 01650h                               ; e8 11 c6
    mov cl, al                                ; 88 c1
    lea dx, [bx+001h]                         ; 8d 57 01
    mov ax, strict word 00040h                ; b8 40 00
    call 01650h                               ; e8 06 c6
    mov es, [bp-008h]                         ; 8e 46 f8
    mov byte [es:si], cl                      ; 26 88 0c
    mov es, [bp-006h]                         ; 8e 46 fa
    mov byte [es:di], al                      ; 26 88 05
    cmp word [bp+004h], strict byte 00000h    ; 83 7e 04 00
    je short 0506fh                           ; 74 13
    inc bx                                    ; 43
    inc bx                                    ; 43
    cmp bx, strict byte 0003eh                ; 83 fb 3e
    jc short 05066h                           ; 72 03
    mov bx, strict word 0001eh                ; bb 1e 00
    mov dx, strict word 0001ah                ; ba 1a 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0167ah                               ; e8 0b c6
    mov ax, strict word 00001h                ; b8 01 00
    jmp short 05076h                          ; eb 02
    xor ax, ax                                ; 31 c0
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn 00002h                               ; c2 02 00
    mov byte [01292h], AL                     ; a2 92 12
    adc word [bx+si], dx                      ; 11 10
    or cl, byte [bx+di]                       ; 0a 09
    add ax, 00102h                            ; 05 02 01
    add byte [bx+si+053h], al                 ; 00 40 53
    push si                                   ; 56
    push cx                                   ; 51
    popfw                                     ; 9d
    push cx                                   ; 51
    jmp near 049e4h                           ; e9 51 f9
    push cx                                   ; 51
    and dx, word [bp+si+02ch]                 ; 23 52 2c
    push dx                                   ; 52
    popfw                                     ; 9d
    push dx                                   ; 52
    into                                      ; ce
    push dx                                   ; 52
    sti                                       ; fb
    push dx                                   ; 52
    xor ax, 08353h                            ; 35 53 83
    push bx                                   ; 53
_int16_function:                             ; 0xf50a2 LB 0x2e7
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push di                                   ; 57
    sub sp, strict byte 00006h                ; 83 ec 06
    mov dx, strict word 00017h                ; ba 17 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01650h                               ; e8 9e c5
    mov cl, al                                ; 88 c1
    mov bh, al                                ; 88 c7
    mov dx, 00097h                            ; ba 97 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01650h                               ; e8 91 c5
    mov bl, al                                ; 88 c3
    movzx dx, cl                              ; 0f b6 d1
    sar dx, 004h                              ; c1 fa 04
    and dl, 007h                              ; 80 e2 07
    and AL, strict byte 007h                  ; 24 07
    xor ah, ah                                ; 30 e4
    xor al, dl                                ; 30 d0
    test ax, ax                               ; 85 c0
    je short 05134h                           ; 74 60
    cli                                       ; fa
    mov AL, strict byte 0edh                  ; b0 ed
    mov dx, strict word 00060h                ; ba 60 00
    out DX, AL                                ; ee
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 050edh                          ; 75 08
    mov AL, strict byte 021h                  ; b0 21
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 050dbh                          ; eb ee
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp ax, 000fah                            ; 3d fa 00
    jne short 05133h                          ; 75 3b
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
    jne short 05122h                          ; 75 08
    mov AL, strict byte 021h                  ; b0 21
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 05110h                          ; eb ee
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    xor bh, bh                                ; 30 ff
    mov dx, 00097h                            ; ba 97 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 2b c5
    sti                                       ; fb
    mov ax, word [bp+012h]                    ; 8b 46 12
    shr ax, 008h                              ; c1 e8 08
    cmp ax, 000a2h                            ; 3d a2 00
    jnbe near 05340h                          ; 0f 87 ff 01
    push CS                                   ; 0e
    pop ES                                    ; 07
    mov cx, strict word 0000ch                ; b9 0c 00
    mov di, 0507fh                            ; bf 7f 50
    repne scasb                               ; f2 ae
    sal cx, 1                                 ; d1 e1
    mov di, cx                                ; 89 cf
    mov ax, word [cs:di+0508ah]               ; 2e 8b 85 8a 50
    jmp ax                                    ; ff e0
    push strict byte 00001h                   ; 6a 01
    mov cx, ss                                ; 8c d1
    lea bx, [bp-008h]                         ; 8d 5e f8
    mov dx, ss                                ; 8c d2
    lea ax, [bp-006h]                         ; 8d 46 fa
    call 0500eh                               ; e8 a9 fe
    test ax, ax                               ; 85 c0
    jne short 05174h                          ; 75 0b
    push 005efh                               ; 68 ef 05
    push strict byte 00007h                   ; 6a 07
    call 01972h                               ; e8 01 c8
    add sp, strict byte 00004h                ; 83 c4 04
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00
    je short 05180h                           ; 74 06
    cmp byte [bp-008h], 0f0h                  ; 80 7e f8 f0
    je short 05186h                           ; 74 06
    cmp byte [bp-008h], 0e0h                  ; 80 7e f8 e0
    jne short 0518ah                          ; 75 04
    mov byte [bp-008h], 000h                  ; c6 46 f8 00
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa
    sal dx, 008h                              ; c1 e2 08
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    or dx, ax                                 ; 09 c2
    mov word [bp+012h], dx                    ; 89 56 12
    jmp near 05383h                           ; e9 e6 01
    or word [bp+01ch], 00200h                 ; 81 4e 1c 00 02
    push strict byte 00000h                   ; 6a 00
    mov cx, ss                                ; 8c d1
    lea bx, [bp-008h]                         ; 8d 5e f8
    mov dx, ss                                ; 8c d2
    lea ax, [bp-006h]                         ; 8d 46 fa
    call 0500eh                               ; e8 5d fe
    test ax, ax                               ; 85 c0
    jne short 051bch                          ; 75 07
    or word [bp+01ch], strict byte 00040h     ; 83 4e 1c 40
    jmp near 05383h                           ; e9 c7 01
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00
    je short 051c8h                           ; 74 06
    cmp byte [bp-008h], 0f0h                  ; 80 7e f8 f0
    je short 051ceh                           ; 74 06
    cmp byte [bp-008h], 0e0h                  ; 80 7e f8 e0
    jne short 051d2h                          ; 75 04
    mov byte [bp-008h], 000h                  ; c6 46 f8 00
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa
    sal dx, 008h                              ; c1 e2 08
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    or dx, ax                                 ; 09 c2
    mov word [bp+012h], dx                    ; 89 56 12
    and word [bp+01ch], strict byte 0ffbfh    ; 83 66 1c bf
    jmp near 05383h                           ; e9 9a 01
    mov dx, strict word 00017h                ; ba 17 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01650h                               ; e8 5e c4
    mov dx, word [bp+012h]                    ; 8b 56 12
    mov dl, al                                ; 88 c2
    jmp short 05197h                          ; eb 9e
    mov al, byte [bp+010h]                    ; 8a 46 10
    movzx dx, al                              ; 0f b6 d0
    mov ax, word [bp+010h]                    ; 8b 46 10
    shr ax, 008h                              ; c1 e8 08
    xor ah, ah                                ; 30 e4
    call 04c1eh                               ; e8 14 fa
    test ax, ax                               ; 85 c0
    jne short 0521bh                          ; 75 0d
    mov ax, word [bp+012h]                    ; 8b 46 12
    xor al, al                                ; 30 c0
    or AL, strict byte 001h                   ; 0c 01
    mov word [bp+012h], ax                    ; 89 46 12
    jmp near 05383h                           ; e9 68 01
    and word [bp+012h], 0ff00h                ; 81 66 12 00 ff
    jmp near 05383h                           ; e9 60 01
    mov ax, word [bp+012h]                    ; 8b 46 12
    xor al, al                                ; 30 c0
    or AL, strict byte 030h                   ; 0c 30
    jmp short 05215h                          ; eb e9
    mov byte [bp-004h], 002h                  ; c6 46 fc 02
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
    jne short 05253h                          ; 75 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 05253h                          ; 76 08
    mov dx, 00080h                            ; ba 80 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    jmp short 0523ch                          ; eb e9
    test bx, bx                               ; 85 db
    jbe short 05297h                          ; 76 40
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp ax, 000fah                            ; 3d fa 00
    jne short 05297h                          ; 75 35
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 0527ch                          ; 75 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 0527ch                          ; 76 08
    mov dx, 00080h                            ; ba 80 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    jmp short 05265h                          ; eb e9
    test bx, bx                               ; 85 db
    jbe short 0528eh                          ; 76 0e
    shr cx, 008h                              ; c1 e9 08
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    sal ax, 008h                              ; c1 e0 08
    or cx, ax                                 ; 09 c1
    dec byte [bp-004h]                        ; fe 4e fc
    cmp byte [bp-004h], 000h                  ; 80 7e fc 00
    jnbe short 05262h                         ; 77 cb
    mov word [bp+00ch], cx                    ; 89 4e 0c
    jmp near 05383h                           ; e9 e6 00
    push strict byte 00001h                   ; 6a 01
    mov cx, ss                                ; 8c d1
    lea bx, [bp-008h]                         ; 8d 5e f8
    mov dx, ss                                ; 8c d2
    lea ax, [bp-006h]                         ; 8d 46 fa
    call 0500eh                               ; e8 62 fd
    test ax, ax                               ; 85 c0
    jne short 052bbh                          ; 75 0b
    push 005efh                               ; 68 ef 05
    push strict byte 00007h                   ; 6a 07
    call 01972h                               ; e8 ba c6
    add sp, strict byte 00004h                ; 83 c4 04
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00
    je near 0518ah                            ; 0f 84 c7 fe
    cmp byte [bp-008h], 0f0h                  ; 80 7e f8 f0
    je near 05186h                            ; 0f 84 bb fe
    jmp near 0518ah                           ; e9 bc fe
    or word [bp+01ch], 00200h                 ; 81 4e 1c 00 02
    push strict byte 00000h                   ; 6a 00
    mov cx, ss                                ; 8c d1
    lea bx, [bp-008h]                         ; 8d 5e f8
    mov dx, ss                                ; 8c d2
    lea ax, [bp-006h]                         ; 8d 46 fa
    call 0500eh                               ; e8 2c fd
    test ax, ax                               ; 85 c0
    je near 051b5h                            ; 0f 84 cd fe
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00
    je near 051d2h                            ; 0f 84 e2 fe
    cmp byte [bp-008h], 0f0h                  ; 80 7e f8 f0
    je near 051ceh                            ; 0f 84 d6 fe
    jmp near 051d2h                           ; e9 d7 fe
    mov dx, strict word 00017h                ; ba 17 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01650h                               ; e8 4c c3
    mov dx, word [bp+012h]                    ; 8b 56 12
    mov dl, al                                ; 88 c2
    mov word [bp+012h], dx                    ; 89 56 12
    mov dx, strict word 00018h                ; ba 18 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01650h                               ; e8 3b c3
    mov bh, al                                ; 88 c7
    and bh, 073h                              ; 80 e7 73
    mov dx, 00096h                            ; ba 96 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01650h                               ; e8 2d c3
    and AL, strict byte 00ch                  ; 24 0c
    or bh, al                                 ; 08 c7
    mov dx, word [bp+012h]                    ; 8b 56 12
    xor dh, dh                                ; 30 f6
    movzx ax, bh                              ; 0f b6 c7
    sal ax, 008h                              ; c1 e0 08
    jmp near 05195h                           ; e9 60 fe
    mov ax, word [bp+012h]                    ; 8b 46 12
    xor ah, ah                                ; 30 e4
    or ah, 080h                               ; 80 cc 80
    jmp near 05215h                           ; e9 d5 fe
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 e6 c5
    mov ax, word [bp+012h]                    ; 8b 46 12
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 00613h                               ; 68 13 06
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 18 c6
    add sp, strict byte 00006h                ; 83 c4 06
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 c9 c5
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    push ax                                   ; 50
    mov ax, word [bp+010h]                    ; 8b 46 10
    push ax                                   ; 50
    mov ax, word [bp+00ch]                    ; 8b 46 0c
    push ax                                   ; 50
    mov ax, word [bp+012h]                    ; 8b 46 12
    push ax                                   ; 50
    push 0063bh                               ; 68 3b 06
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 f2 c5
    add sp, strict byte 0000ch                ; 83 c4 0c
    lea sp, [bp-002h]                         ; 8d 66 fe
    pop di                                    ; 5f
    pop bp                                    ; 5d
    retn                                      ; c3
set_geom_lba_:                               ; 0xf5389 LB 0x9e
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
    jmp short 053aah                          ; eb 05
    cmp bx, strict byte 00004h                ; 83 fb 04
    jnl short 053c0h                          ; 7d 16
    cmp dx, word [bp-002h]                    ; 3b 56 fe
    jnbe short 053b6h                         ; 77 07
    jne short 053b9h                          ; 75 08
    cmp ax, word [bp-004h]                    ; 3b 46 fc
    jc short 053b9h                           ; 72 03
    inc di                                    ; 47
    shr di, 1                                 ; d1 ef
    shr dx, 1                                 ; d1 ea
    rcr ax, 1                                 ; d1 d8
    inc bx                                    ; 43
    jmp short 053a5h                          ; eb e5
    mov ax, di                                ; 89 f8
    xor dx, dx                                ; 31 d2
    mov bx, strict word 0003fh                ; bb 3f 00
    xor cx, cx                                ; 31 c9
    call 094a9h                               ; e8 dd 40
    mov bx, ax                                ; 89 c3
    mov cx, dx                                ; 89 d1
    mov ax, word [bp-004h]                    ; 8b 46 fc
    mov dx, word [bp-002h]                    ; 8b 56 fe
    call 09470h                               ; e8 97 40
    mov word [es:si+002h], ax                 ; 26 89 44 02
    cmp ax, 00400h                            ; 3d 00 04
    jbe short 053e8h                          ; 76 06
    mov word [es:si+002h], 00400h             ; 26 c7 44 02 00 04
    mov word [es:si], di                      ; 26 89 3c
    mov word [es:si+004h], strict word 0003fh ; 26 c7 44 04 3f 00
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
    mov CL, strict byte 054h                  ; b1 54
    xlatb                                     ; d7
    push sp                                   ; 54
    add AL, strict byte 055h                  ; 04 55
    add AL, strict byte 055h                  ; 04 55
    add AL, strict byte 055h                  ; 04 55
    fcom qword [bp+00ah]                      ; dc 56 0a
    pop ax                                    ; 58
    or bl, byte [bx+si-00bh]                  ; 0a 58 f5
    push si                                   ; 56
    out 057h, ax                              ; e7 57
    or bl, byte [bx+si+00ah]                  ; 0a 58 0a
    pop ax                                    ; 58
    out 057h, ax                              ; e7 57
    out 057h, ax                              ; e7 57
    or bl, byte [bx+si+00ah]                  ; 0a 58 0a
    pop ax                                    ; 58
    imul dx, word [bx-019h], strict byte 00057h ; 6b 57 e7 57
    or bl, byte [bx+si+00ah]                  ; 0a 58 0a
    pop ax                                    ; 58
    out 057h, ax                              ; e7 57
    wait                                      ; 9b
    push di                                   ; 57
    or bl, byte [bx+si+00ah]                  ; 0a 58 0a
    pop ax                                    ; 58
    db  00ah
    pop ax                                    ; 58
_int13_harddisk:                             ; 0xf5427 LB 0x441
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sub sp, strict byte 00010h                ; 83 ec 10
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 36 c2
    mov si, 00122h                            ; be 22 01
    mov word [bp-004h], ax                    ; 89 46 fc
    xor bx, bx                                ; 31 db
    mov dx, 0008eh                            ; ba 8e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 17 c2
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    xor ah, ah                                ; 30 e4
    cmp ax, 00080h                            ; 3d 80 00
    jc short 05456h                           ; 72 05
    cmp ax, 00090h                            ; 3d 90 00
    jc short 05474h                           ; 72 1e
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 0065eh                               ; 68 5e 06
    push 0066dh                               ; 68 6d 06
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 04 c5
    add sp, strict byte 0000ah                ; 83 c4 0a
    jmp near 05825h                           ; e9 b1 03
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    xor ah, ah                                ; 30 e4
    mov es, [bp-004h]                         ; 8e 46 fc
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    mov dl, byte [es:bx+0011fh]               ; 26 8a 97 1f 01
    mov byte [bp-002h], dl                    ; 88 56 fe
    cmp dl, 010h                              ; 80 fa 10
    jc short 0549dh                           ; 72 10
    push ax                                   ; 50
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 0065eh                               ; 68 5e 06
    push 00698h                               ; 68 98 06
    jmp short 05469h                          ; eb cc
    mov bx, word [bp+016h]                    ; 8b 5e 16
    shr bx, 008h                              ; c1 eb 08
    cmp bx, strict byte 00018h                ; 83 fb 18
    jnbe near 0580ah                          ; 0f 87 60 03
    add bx, bx                                ; 01 db
    jmp word [cs:bx+053f5h]                   ; 2e ff a7 f5 53
    cmp byte [bp-002h], 008h                  ; 80 7e fe 08
    jnc near 054c0h                           ; 0f 83 07 00
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    call 01c71h                               ; e8 b1 c7
    mov byte [bp+017h], 000h                  ; c6 46 17 00
    xor bx, bx                                ; 31 db
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 8f c1
    and byte [bp+01ch], 0feh                  ; 80 66 1c fe
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    retn                                      ; c3
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01650h                               ; e8 70 c1
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
    call 0165eh                               ; e8 61 c1
    test cl, cl                               ; 84 c9
    je short 054c4h                           ; 74 c3
    jmp near 05841h                           ; e9 3d 03
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
    jnbe short 0553fh                         ; 77 04
    test ax, ax                               ; 85 c0
    jne short 05562h                          ; 75 23
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 e7 c3
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 0065eh                               ; 68 5e 06
    push 006cah                               ; 68 ca 06
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 16 c4
    add sp, strict byte 00008h                ; 83 c4 08
    jmp near 05825h                           ; e9 c3 02
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
    jnc short 0558fh                          ; 73 0c
    cmp cx, word [bp-008h]                    ; 3b 4e f8
    jbe short 0558fh                          ; 76 07
    mov ax, word [bp-006h]                    ; 8b 46 fa
    cmp ax, dx                                ; 39 d0
    jbe short 055bdh                          ; 76 2e
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 97 c3
    push dword [bp-008h]                      ; 66 ff 76 f8
    push di                                   ; 57
    mov ax, word [bp+012h]                    ; 8b 46 12
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 0065eh                               ; 68 5e 06
    push 006f2h                               ; 68 f2 06
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 bb c3
    add sp, strict byte 00010h                ; 83 c4 10
    jmp near 05825h                           ; e9 68 02
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    cmp ax, strict word 00004h                ; 3d 04 00
    jne short 055cbh                          ; 75 03
    jmp near 054c0h                           ; e9 f5 fe
    movzx bx, byte [bp-002h]                  ; 0f b6 5e fe
    imul bx, bx, strict byte 00018h           ; 6b db 18
    mov es, [bp-004h]                         ; 8e 46 fc
    add bx, si                                ; 01 f3
    cmp cx, word [es:bx+02ch]                 ; 26 3b 4f 2c
    jne short 055ech                          ; 75 0f
    mov ax, word [es:bx+030h]                 ; 26 8b 47 30
    cmp ax, word [bp-00ah]                    ; 3b 46 f6
    jne short 055ech                          ; 75 06
    cmp byte [bp-002h], 008h                  ; 80 7e fe 08
    jc short 0561ch                           ; 72 30
    mov ax, di                                ; 89 f8
    xor dx, dx                                ; 31 d2
    mov bx, cx                                ; 89 cb
    xor cx, cx                                ; 31 c9
    call 094a9h                               ; e8 b2 3e
    xor bx, bx                                ; 31 db
    add ax, word [bp-008h]                    ; 03 46 f8
    adc dx, bx                                ; 11 da
    mov bx, word [bp-00ah]                    ; 8b 5e f6
    xor cx, cx                                ; 31 c9
    call 094a9h                               ; e8 a3 3e
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
    call word [word bx+0007eh]                ; ff 97 7e 00
    mov dx, ax                                ; 89 c2
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor al, al                                ; 30 c0
    mov es, [bp-004h]                         ; 8e 46 fc
    mov bx, word [es:si+014h]                 ; 26 8b 5c 14
    or bx, ax                                 ; 09 c3
    mov word [bp+016h], bx                    ; 89 5e 16
    test dl, dl                               ; 84 d2
    je near 054c0h                            ; 0f 84 13 fe
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 79 c2
    movzx ax, dl                              ; 0f b6 c2
    push ax                                   ; 50
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 0065eh                               ; 68 5e 06
    push 00739h                               ; 68 39 07
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 a4 c2
    add sp, strict byte 0000ah                ; 83 c4 0a
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    or ah, 00ch                               ; 80 cc 0c
    jmp near 0582dh                           ; e9 51 01
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 4a c2
    push 0075ah                               ; 68 5a 07
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 83 c2
    add sp, strict byte 00004h                ; 83 c4 04
    jmp near 054c0h                           ; e9 cb fd
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
    jmp near 054c0h                           ; e9 55 fd
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
    jne short 05790h                          ; 75 03
    jmp near 054c0h                           ; e9 30 fd
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    or ah, 0aah                               ; 80 cc aa
    jmp near 0582dh                           ; e9 92 00
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
    call 094a9h                               ; e8 e4 3c
    mov bx, word [bp-006h]                    ; 8b 5e fa
    xor cx, cx                                ; 31 c9
    call 094a9h                               ; e8 dc 3c
    mov word [bp-010h], ax                    ; 89 46 f0
    mov word [bp-00eh], dx                    ; 89 56 f2
    mov word [bp+014h], dx                    ; 89 56 14
    mov word [bp+012h], ax                    ; 89 46 12
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    or ah, 003h                               ; 80 cc 03
    mov word [bp+016h], ax                    ; 89 46 16
    jmp near 054c4h                           ; e9 dd fc
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 3f c1
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 0065eh                               ; 68 5e 06
    push 00774h                               ; 68 74 07
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 6e c1
    add sp, strict byte 00008h                ; 83 c4 08
    jmp near 054c0h                           ; e9 b6 fc
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 1c c1
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 0065eh                               ; 68 5e 06
    push 007a7h                               ; 68 a7 07
    jmp near 05557h                           ; e9 32 fd
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    or ah, 001h                               ; 80 cc 01
    mov word [bp+016h], ax                    ; 89 46 16
    mov bx, word [bp+016h]                    ; 8b 5e 16
    shr bx, 008h                              ; c1 eb 08
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 1d be
    or byte [bp+01ch], 001h                   ; 80 4e 1c 01
    jmp near 054d3h                           ; e9 8b fc
    add bx, word [bx+di+01bh]                 ; 03 59 1b
    pop cx                                    ; 59
    sbb bx, word [bx+di+01bh]                 ; 1b 59 1b
    pop cx                                    ; 59
    retf 05e5ch                               ; ca 5c 5e
    pop dx                                    ; 5a
    sbb bx, word [bx+di+064h]                 ; 1b 59 64
    pop dx                                    ; 5a
    retf 0195ch                               ; ca 5c 19
    pop bp                                    ; 5d
    sbb word [di+019h], bx                    ; 19 5d 19
    pop bp                                    ; 5d
    sbb word [di-01fh], bx                    ; 19 5d e1
    pop sp                                    ; 5c
    sbb word [di+019h], bx                    ; 19 5d 19
    pop bp                                    ; 5d
_int13_harddisk_ext:                         ; 0xf5868 LB 0x4cc
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sub sp, strict byte 00028h                ; 83 ec 28
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 f5 bd
    mov word [bp-014h], ax                    ; 89 46 ec
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 e9 bd
    mov si, 00122h                            ; be 22 01
    mov word [bp-026h], ax                    ; 89 46 da
    xor bx, bx                                ; 31 db
    mov dx, 0008eh                            ; ba 8e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 ca bd
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    xor ah, ah                                ; 30 e4
    cmp ax, 00080h                            ; 3d 80 00
    jc short 058a3h                           ; 72 05
    cmp ax, 00090h                            ; 3d 90 00
    jc short 058c1h                           ; 72 1e
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 007d5h                               ; 68 d5 07
    push 0066dh                               ; 68 6d 06
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 b7 c0
    add sp, strict byte 0000ah                ; 83 c4 0a
    jmp near 05cf7h                           ; e9 36 04
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    xor ah, ah                                ; 30 e4
    mov es, [bp-026h]                         ; 8e 46 da
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    mov cl, byte [es:bx+0011fh]               ; 26 8a 8f 1f 01
    cmp cl, 010h                              ; 80 f9 10
    jc short 058e7h                           ; 72 10
    push ax                                   ; 50
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 007d5h                               ; 68 d5 07
    push 00698h                               ; 68 98 06
    jmp short 058b6h                          ; eb cf
    mov bx, word [bp+016h]                    ; 8b 5e 16
    shr bx, 008h                              ; c1 eb 08
    sub bx, strict byte 00041h                ; 83 eb 41
    cmp bx, strict byte 0000fh                ; 83 fb 0f
    jnbe near 05d19h                          ; 0f 87 22 04
    add bx, bx                                ; 01 db
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    jmp word [cs:bx+05848h]                   ; 2e ff a7 48 58
    mov word [bp+010h], 0aa55h                ; c7 46 10 55 aa
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    or ah, 030h                               ; 80 cc 30
    mov word [bp+016h], ax                    ; 89 46 16
    mov word [bp+014h], strict word 00007h    ; c7 46 14 07 00
    jmp near 05cceh                           ; e9 b3 03
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
    je short 0595ch                           ; 74 11
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 007d5h                               ; 68 d5 07
    push 007e8h                               ; 68 e8 07
    push strict byte 00007h                   ; 6a 07
    jmp short 059a6h                          ; eb 4a
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
    jnbe short 0598ch                         ; 77 0b
    jne short 059afh                          ; 75 2c
    mov dx, word [bp-00ah]                    ; 8b 56 f6
    cmp dx, word [es:bx+032h]                 ; 26 3b 57 32
    jc short 059afh                           ; 72 23
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 9a bf
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 007d5h                               ; 68 d5 07
    push 00811h                               ; 68 11 08
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 c9 bf
    add sp, strict byte 00008h                ; 83 c4 08
    jmp near 05cf7h                           ; e9 48 03
    mov dx, word [bp+016h]                    ; 8b 56 16
    shr dx, 008h                              ; c1 ea 08
    mov word [bp-00ch], dx                    ; 89 56 f4
    cmp dx, strict byte 00044h                ; 83 fa 44
    je near 05ccah                            ; 0f 84 0b 03
    cmp dx, strict byte 00047h                ; 83 fa 47
    je near 05ccah                            ; 0f 84 04 03
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
    call word [word bx-00002h]                ; ff 97 fe ff
    mov dx, ax                                ; 89 c2
    mov es, [bp-026h]                         ; 8e 46 da
    mov ax, word [es:si+014h]                 ; 26 8b 44 14
    mov word [bp-012h], ax                    ; 89 46 ee
    mov es, [bp-010h]                         ; 8e 46 f0
    mov word [es:di+002h], ax                 ; 26 89 45 02
    test dl, dl                               ; 84 d2
    je near 05ccah                            ; 0f 84 97 02
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 f3 be
    movzx ax, dl                              ; 0f b6 c2
    push ax                                   ; 50
    push word [bp-00ch]                       ; ff 76 f4
    push 007d5h                               ; 68 d5 07
    push 00739h                               ; 68 39 07
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 22 bf
    add sp, strict byte 0000ah                ; 83 c4 0a
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    or ah, 00ch                               ; 80 cc 0c
    jmp near 05cffh                           ; e9 a1 02
    or ah, 0b2h                               ; 80 cc b2
    jmp near 05cffh                           ; e9 9b 02
    mov bx, word [bp+00ah]                    ; 8b 5e 0a
    mov ax, word [bp+004h]                    ; 8b 46 04
    mov word [bp-006h], ax                    ; 89 46 fa
    mov word [bp-004h], ax                    ; 89 46 fc
    mov es, ax                                ; 8e c0
    mov di, bx                                ; 89 df
    mov ax, word [es:di]                      ; 26 8b 05
    mov word [bp-00eh], ax                    ; 89 46 f2
    cmp ax, strict word 0001ah                ; 3d 1a 00
    jc near 05cf7h                            ; 0f 82 76 02
    jc near 05b08h                            ; 0f 82 83 00
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
    jc near 05c14h                            ; 0f 82 04 01
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
    jne short 05b66h                          ; 75 04
    xor dx, dx                                ; 31 d2
    jmp short 05b69h                          ; eb 03
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
    jne short 05b8fh                          ; 75 05
    mov ax, strict word 00003h                ; b8 03 00
    jmp short 05b91h                          ; eb 02
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
    jmp short 05bf5h                          ; eb 06
    cmp byte [bp-002h], 00fh                  ; 80 7e fe 0f
    jnc short 05c0ah                          ; 73 15
    movzx dx, byte [bp-002h]                  ; 0f b6 56 fe
    add dx, 00312h                            ; 81 c2 12 03
    mov ax, word [bp-014h]                    ; 8b 46 ec
    call 01650h                               ; e8 4d ba
    add ch, al                                ; 00 c5
    inc byte [bp-002h]                        ; fe 46 fe
    jmp short 05befh                          ; eb e5
    neg ch                                    ; f6 dd
    mov es, [bp-026h]                         ; 8e 46 da
    mov byte [es:si+001ffh], ch               ; 26 88 ac ff 01
    cmp word [bp-00eh], strict byte 00042h    ; 83 7e f2 42
    jc near 05ccah                            ; 0f 82 ae 00
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
    jne short 05c5ch                          ; 75 09
    db  066h, 026h, 0c7h, 047h, 024h, 049h, 053h, 041h, 020h
    ; mov dword [es:bx+024h], strict dword 020415349h ; 66 26 c7 47 24 49 53 41 20
    mov es, [bp-004h]                         ; 8e 46 fc
    db  066h, 026h, 0c7h, 047h, 028h, 041h, 054h, 041h, 020h
    ; mov dword [es:bx+028h], strict dword 020415441h ; 66 26 c7 47 28 41 54 41 20
    db  066h, 026h, 0c7h, 047h, 02ch, 020h, 020h, 020h, 020h
    ; mov dword [es:bx+02ch], strict dword 020202020h ; 66 26 c7 47 2c 20 20 20 20
    test al, al                               ; 84 c0
    jne short 05c88h                          ; 75 13
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
    jmp short 05cafh                          ; eb 05
    cmp ch, 040h                              ; 80 fd 40
    jnc short 05cc1h                          ; 73 12
    movzx dx, ch                              ; 0f b6 d5
    add dx, word [bp+00ah]                    ; 03 56 0a
    mov ax, word [bp+004h]                    ; 8b 46 04
    call 01650h                               ; e8 95 b9
    add cl, al                                ; 00 c1
    db  0feh, 0c5h
    ; inc ch                                    ; fe c5
    jmp short 05caah                          ; eb e9
    neg cl                                    ; f6 d9
    mov es, [bp-004h]                         ; 8e 46 fc
    mov byte [es:bx+041h], cl                 ; 26 88 4f 41
    mov byte [bp+017h], 000h                  ; c6 46 17 00
    xor bx, bx                                ; 31 db
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 85 b9
    and byte [bp+01ch], 0feh                  ; 80 66 1c fe
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    retn                                      ; c3
    cmp ax, strict word 00006h                ; 3d 06 00
    je short 05ccah                           ; 74 e4
    cmp ax, strict word 00001h                ; 3d 01 00
    jc short 05cf7h                           ; 72 0c
    jbe short 05ccah                          ; 76 dd
    cmp ax, strict word 00003h                ; 3d 03 00
    jc short 05cf7h                           ; 72 05
    cmp ax, strict word 00004h                ; 3d 04 00
    jbe short 05ccah                          ; 76 d3
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    or ah, 001h                               ; 80 cc 01
    mov word [bp+016h], ax                    ; 89 46 16
    mov bx, word [bp+016h]                    ; 8b 5e 16
    shr bx, 008h                              ; c1 eb 08
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 4b b9
    or byte [bp+01ch], 001h                   ; 80 4e 1c 01
    jmp short 05cddh                          ; eb c4
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 0d bc
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    push ax                                   ; 50
    push 007d5h                               ; 68 d5 07
    push 007a7h                               ; 68 a7 07
    jmp near 059a4h                           ; e9 70 fc
_int14_function:                             ; 0xf5d34 LB 0x155
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    sti                                       ; fb
    mov dx, word [bp+00eh]                    ; 8b 56 0e
    add dx, dx                                ; 01 d2
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 28 b9
    mov si, ax                                ; 89 c6
    mov bx, ax                                ; 89 c3
    mov dx, word [bp+00eh]                    ; 8b 56 0e
    add dx, strict byte 0007ch                ; 83 c2 7c
    mov ax, strict word 00040h                ; b8 40 00
    call 01650h                               ; e8 fc b8
    mov cl, al                                ; 88 c1
    cmp word [bp+00eh], strict byte 00004h    ; 83 7e 0e 04
    jnc near 05e7fh                           ; 0f 83 21 01
    test si, si                               ; 85 f6
    jbe near 05e7fh                           ; 0f 86 1b 01
    mov al, byte [bp+013h]                    ; 8a 46 13
    cmp AL, strict byte 001h                  ; 3c 01
    jc short 05d7ch                           ; 72 11
    jbe short 05dd0h                          ; 76 63
    cmp AL, strict byte 003h                  ; 3c 03
    je near 05e68h                            ; 0f 84 f5 00
    cmp AL, strict byte 002h                  ; 3c 02
    je near 05e1eh                            ; 0f 84 a5 00
    jmp near 05e79h                           ; e9 fd 00
    test al, al                               ; 84 c0
    jne near 05e79h                           ; 0f 85 f7 00
    lea dx, [bx+003h]                         ; 8d 57 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    or AL, strict byte 080h                   ; 0c 80
    out DX, AL                                ; ee
    mov al, byte [bp+012h]                    ; 8a 46 12
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
    mov al, byte [bp+012h]                    ; 8a 46 12
    and AL, strict byte 01fh                  ; 24 1f
    lea dx, [bx+003h]                         ; 8d 57 03
    out DX, AL                                ; ee
    lea dx, [bx+005h]                         ; 8d 57 05
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov byte [bp+013h], al                    ; 88 46 13
    lea dx, [bx+006h]                         ; 8d 57 06
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov byte [bp+012h], al                    ; 88 46 12
    jmp near 05e5ah                           ; e9 97 00
    mov AL, strict byte 017h                  ; b0 17
    mov dx, bx                                ; 89 da
    out DX, AL                                ; ee
    lea dx, [bx+001h]                         ; 8d 57 01
    mov AL, strict byte 004h                  ; b0 04
    out DX, AL                                ; ee
    jmp short 05da5h                          ; eb d5
    mov dx, strict word 0006ch                ; ba 6c 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 93 b8
    mov si, ax                                ; 89 c6
    lea dx, [bx+005h]                         ; 8d 57 05
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and ax, strict word 00060h                ; 25 60 00
    cmp ax, strict word 00060h                ; 3d 60 00
    je short 05e00h                           ; 74 17
    test cl, cl                               ; 84 c9
    je short 05e00h                           ; 74 13
    mov dx, strict word 0006ch                ; ba 6c 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 76 b8
    cmp ax, si                                ; 39 f0
    je short 05ddbh                           ; 74 e1
    mov si, ax                                ; 89 c6
    db  0feh, 0c9h
    ; dec cl                                    ; fe c9
    jmp short 05ddbh                          ; eb db
    test cl, cl                               ; 84 c9
    je short 05e0ah                           ; 74 06
    mov al, byte [bp+012h]                    ; 8a 46 12
    mov dx, bx                                ; 89 da
    out DX, AL                                ; ee
    lea dx, [bx+005h]                         ; 8d 57 05
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov byte [bp+013h], al                    ; 88 46 13
    test cl, cl                               ; 84 c9
    jne short 05e5ah                          ; 75 43
    or AL, strict byte 080h                   ; 0c 80
    mov byte [bp+013h], al                    ; 88 46 13
    jmp short 05e5ah                          ; eb 3c
    mov dx, strict word 0006ch                ; ba 6c 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 45 b8
    mov si, ax                                ; 89 c6
    lea dx, [bx+005h]                         ; 8d 57 05
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 05e4ah                          ; 75 17
    test cl, cl                               ; 84 c9
    je short 05e4ah                           ; 74 13
    mov dx, strict word 0006ch                ; ba 6c 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 2c b8
    cmp ax, si                                ; 39 f0
    je short 05e29h                           ; 74 e5
    mov si, ax                                ; 89 c6
    db  0feh, 0c9h
    ; dec cl                                    ; fe c9
    jmp short 05e29h                          ; eb df
    test cl, cl                               ; 84 c9
    je short 05e60h                           ; 74 12
    mov byte [bp+013h], 000h                  ; c6 46 13 00
    mov dx, bx                                ; 89 da
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov byte [bp+012h], al                    ; 88 46 12
    and byte [bp+01ch], 0feh                  ; 80 66 1c fe
    jmp short 05e83h                          ; eb 23
    lea dx, [bx+005h]                         ; 8d 57 05
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    jmp short 05e19h                          ; eb b1
    lea dx, [si+005h]                         ; 8d 54 05
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov byte [bp+013h], al                    ; 88 46 13
    lea dx, [si+006h]                         ; 8d 54 06
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    jmp short 05e57h                          ; eb de
    or byte [bp+01ch], 001h                   ; 80 4e 1c 01
    jmp short 05e83h                          ; eb 04
    or byte [bp+01ch], 001h                   ; 80 4e 1c 01
    lea sp, [bp-002h]                         ; 8d 66 fe
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn                                      ; c3
set_enable_a20_:                             ; 0xf5e89 LB 0x2c
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    mov bx, ax                                ; 89 c3
    mov dx, 00092h                            ; ba 92 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov cl, al                                ; 88 c1
    test bx, bx                               ; 85 db
    je short 05ea2h                           ; 74 05
    or AL, strict byte 002h                   ; 0c 02
    out DX, AL                                ; ee
    jmp short 05ea5h                          ; eb 03
    and AL, strict byte 0fdh                  ; 24 fd
    out DX, AL                                ; ee
    test cl, 002h                             ; f6 c1 02
    db  00fh, 095h, 0c0h
    ; setne al                                  ; 0f 95 c0
    xor ah, ah                                ; 30 e4
    lea sp, [bp-006h]                         ; 8d 66 fa
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
set_e820_range_:                             ; 0xf5eb5 LB 0x8c
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
    jmp near 020e8h                           ; e9 d8 c1
    sar byte [bx-06f6fh], 089h                ; c0 bf 91 90 89
    mov byte [bx+05283h], al                  ; 88 87 83 52
    dec di                                    ; 4f
    inc cx                                    ; 41
    and AL, strict byte 000h                  ; 24 00
    loopne 05f82h                             ; e0 63
    sbb byte [bx-06dh], 05fh                  ; 80 5f 93 5f
    sub byte [bx+si+02eh], ah                 ; 28 60 2e
    pushaw                                    ; 60
    xor sp, word [bx+si+038h]                 ; 33 60 38
    pushaw                                    ; 60
    fisub dword [bx+si+077h]                  ; da 60 77
    bound bx, [di+02162h]                     ; 62 9d 62 21
    pushaw                                    ; 60
    and word [bx+si+06ah], sp                 ; 21 60 6a
    arpl word [bp+si-05a9dh], dx              ; 63 92 63 a5
    arpl word [si+02863h], si                 ; 63 b4 63 28
    pushaw                                    ; 60
    db  0bbh
    db  063h
_int15_function:                             ; 0xf5f41 LB 0x4cd
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    push di                                   ; 57
    push ax                                   ; 50
    push ax                                   ; 50
    mov ax, word [bp+012h]                    ; 8b 46 12
    shr ax, 008h                              ; c1 e8 08
    cmp ax, 000ech                            ; 3d ec 00
    jnbe near 063e0h                          ; 0f 87 8b 04
    push CS                                   ; 0e
    pop ES                                    ; 07
    mov cx, strict word 00012h                ; b9 12 00
    mov di, 05f0ch                            ; bf 0c 5f
    repne scasb                               ; f2 ae
    sal cx, 1                                 ; d1 e1
    mov di, cx                                ; 89 cf
    mov si, word [cs:di+05f1dh]               ; 2e 8b b5 1d 5f
    mov ax, word [bp+012h]                    ; 8b 46 12
    xor ah, ah                                ; 30 e4
    mov cx, word [bp+018h]                    ; 8b 4e 18
    and cl, 0feh                              ; 80 e1 fe
    mov bx, word [bp+018h]                    ; 8b 5e 18
    or bl, 001h                               ; 80 cb 01
    mov dx, ax                                ; 89 c2
    or dh, 086h                               ; 80 ce 86
    jmp si                                    ; ff e6
    mov ax, word [bp+012h]                    ; 8b 46 12
    xor ah, ah                                ; 30 e4
    cmp ax, 000c0h                            ; 3d c0 00
    jne near 063e0h                           ; 0f 85 54 04
    or byte [bp+018h], 001h                   ; 80 4e 18 01
    jmp near 06389h                           ; e9 f6 03
    mov dx, ax                                ; 89 c2
    cmp ax, strict word 00001h                ; 3d 01 00
    jc short 05fa8h                           ; 72 0e
    jbe short 05fbch                          ; 76 20
    cmp ax, strict word 00003h                ; 3d 03 00
    je short 05fe9h                           ; 74 48
    cmp ax, strict word 00002h                ; 3d 02 00
    je short 05fcch                           ; 74 26
    jmp short 05ff6h                          ; eb 4e
    test ax, ax                               ; 85 c0
    jne short 05ff6h                          ; 75 4a
    xor ax, ax                                ; 31 c0
    call 05e89h                               ; e8 d8 fe
    and byte [bp+018h], 0feh                  ; 80 66 18 fe
    mov byte [bp+013h], 000h                  ; c6 46 13 00
    jmp near 06021h                           ; e9 65 00
    mov ax, strict word 00001h                ; b8 01 00
    call 05e89h                               ; e8 c7 fe
    and byte [bp+018h], 0feh                  ; 80 66 18 fe
    mov byte [bp+013h], dh                    ; 88 76 13
    jmp near 06021h                           ; e9 55 00
    mov dx, 00092h                            ; ba 92 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    shr ax, 1                                 ; d1 e8
    and ax, strict word 00001h                ; 25 01 00
    mov dx, word [bp+012h]                    ; 8b 56 12
    mov dl, al                                ; 88 c2
    mov word [bp+012h], dx                    ; 89 56 12
    and byte [bp+018h], 0feh                  ; 80 66 18 fe
    mov byte [bp+013h], ah                    ; 88 66 13
    jmp near 06021h                           ; e9 38 00
    and byte [bp+018h], 0feh                  ; 80 66 18 fe
    mov byte [bp+013h], ah                    ; 88 66 13
    mov word [bp+00ch], ax                    ; 89 46 0c
    jmp near 06021h                           ; e9 2b 00
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 30 b9
    mov ax, word [bp+012h]                    ; 8b 46 12
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    push 00836h                               ; 68 36 08
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 63 b9
    add sp, strict byte 00006h                ; 83 c4 06
    or byte [bp+018h], 001h                   ; 80 4e 18 01
    mov ax, word [bp+012h]                    ; 8b 46 12
    xor ah, ah                                ; 30 e4
    or ah, 086h                               ; 80 cc 86
    mov word [bp+012h], ax                    ; 89 46 12
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn                                      ; c3
    mov word [bp+018h], bx                    ; 89 5e 18
    jmp near 060d4h                           ; e9 a6 00
    mov word [bp+018h], bx                    ; 89 5e 18
    jmp short 06021h                          ; eb ee
    mov word [bp+018h], cx                    ; 89 4e 18
    jmp short 0601eh                          ; eb e6
    test byte [bp+012h], 0ffh                 ; f6 46 12 ff
    je short 060aah                           ; 74 6c
    mov dx, 000a0h                            ; ba a0 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01650h                               ; e8 09 b6
    test AL, strict byte 001h                 ; a8 01
    jne near 06380h                           ; 0f 85 33 03
    mov bx, strict word 00001h                ; bb 01 00
    mov dx, 000a0h                            ; ba a0 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 05 b6
    mov bx, word [bp+014h]                    ; 8b 5e 14
    mov dx, 00098h                            ; ba 98 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0167ah                               ; e8 15 b6
    mov bx, word [bp+00ch]                    ; 8b 5e 0c
    mov dx, 0009ah                            ; ba 9a 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0167ah                               ; e8 09 b6
    mov bx, word [bp+00eh]                    ; 8b 5e 0e
    mov dx, 0009ch                            ; ba 9c 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0167ah                               ; e8 fd b5
    mov bx, word [bp+010h]                    ; 8b 5e 10
    mov dx, 0009eh                            ; ba 9e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0167ah                               ; e8 f1 b5
    and byte [bp+018h], 0feh                  ; 80 66 18 fe
    mov dx, 000a1h                            ; ba a1 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and AL, strict byte 0feh                  ; 24 fe
    out DX, AL                                ; ee
    mov ax, strict word 0000bh                ; b8 0b 00
    call 016ach                               ; e8 10 b6
    or AL, strict byte 040h                   ; 0c 40
    movzx dx, al                              ; 0f b6 d0
    mov ax, strict word 0000bh                ; b8 0b 00
    call 016c9h                               ; e8 22 b6
    jmp near 06021h                           ; e9 77 ff
    cmp ax, strict word 00001h                ; 3d 01 00
    jne short 060c8h                          ; 75 19
    xor bx, bx                                ; 31 db
    mov dx, 000a0h                            ; ba a0 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 a4 b5
    and byte [bp+018h], 0feh                  ; 80 66 18 fe
    mov ax, strict word 0000bh                ; b8 0b 00
    call 016ach                               ; e8 e8 b5
    and AL, strict byte 0bfh                  ; 24 bf
    jmp short 0609eh                          ; eb d6
    mov word [bp+018h], bx                    ; 89 5e 18
    mov ax, dx                                ; 89 d0
    xor ah, dh                                ; 30 f4
    xor dl, dl                                ; 30 d2
    dec ax                                    ; 48
    or dx, ax                                 ; 09 c2
    mov word [bp+012h], dx                    ; 89 56 12
    jmp near 06021h                           ; e9 47 ff
    cli                                       ; fa
    mov ax, strict word 00001h                ; b8 01 00
    call 05e89h                               ; e8 a8 fd
    mov di, ax                                ; 89 c7
    mov ax, word [bp+014h]                    ; 8b 46 14
    sal ax, 004h                              ; c1 e0 04
    mov cx, word [bp+006h]                    ; 8b 4e 06
    add cx, ax                                ; 01 c1
    mov dx, word [bp+014h]                    ; 8b 56 14
    shr dx, 00ch                              ; c1 ea 0c
    mov byte [bp-006h], dl                    ; 88 56 fa
    cmp cx, ax                                ; 39 c1
    jnc short 06100h                          ; 73 05
    db  0feh, 0c2h
    ; inc dl                                    ; fe c2
    mov byte [bp-006h], dl                    ; 88 56 fa
    mov dx, word [bp+006h]                    ; 8b 56 06
    add dx, strict byte 00008h                ; 83 c2 08
    mov ax, word [bp+014h]                    ; 8b 46 14
    mov bx, strict word 0002fh                ; bb 2f 00
    call 0167ah                               ; e8 6b b5
    mov dx, word [bp+006h]                    ; 8b 56 06
    add dx, strict byte 0000ah                ; 83 c2 0a
    mov ax, word [bp+014h]                    ; 8b 46 14
    mov bx, cx                                ; 89 cb
    call 0167ah                               ; e8 5d b5
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa
    mov dx, word [bp+006h]                    ; 8b 56 06
    add dx, strict byte 0000ch                ; 83 c2 0c
    mov ax, word [bp+014h]                    ; 8b 46 14
    call 0165eh                               ; e8 31 b5
    mov dx, word [bp+006h]                    ; 8b 56 06
    add dx, strict byte 0000dh                ; 83 c2 0d
    mov ax, word [bp+014h]                    ; 8b 46 14
    mov bx, 00093h                            ; bb 93 00
    call 0165eh                               ; e8 22 b5
    mov dx, word [bp+006h]                    ; 8b 56 06
    add dx, strict byte 0000eh                ; 83 c2 0e
    mov ax, word [bp+014h]                    ; 8b 46 14
    xor bx, bx                                ; 31 db
    call 0167ah                               ; e8 30 b5
    mov dx, word [bp+006h]                    ; 8b 56 06
    add dx, strict byte 00020h                ; 83 c2 20
    mov ax, word [bp+014h]                    ; 8b 46 14
    mov bx, strict word 0ffffh                ; bb ff ff
    call 0167ah                               ; e8 21 b5
    mov dx, word [bp+006h]                    ; 8b 56 06
    add dx, strict byte 00022h                ; 83 c2 22
    mov ax, word [bp+014h]                    ; 8b 46 14
    xor bx, bx                                ; 31 db
    call 0167ah                               ; e8 13 b5
    mov dx, word [bp+006h]                    ; 8b 56 06
    add dx, strict byte 00024h                ; 83 c2 24
    mov ax, word [bp+014h]                    ; 8b 46 14
    mov bx, strict word 0000fh                ; bb 0f 00
    call 0165eh                               ; e8 e8 b4
    mov dx, word [bp+006h]                    ; 8b 56 06
    add dx, strict byte 00025h                ; 83 c2 25
    mov ax, word [bp+014h]                    ; 8b 46 14
    mov bx, 0009bh                            ; bb 9b 00
    call 0165eh                               ; e8 d9 b4
    mov dx, word [bp+006h]                    ; 8b 56 06
    add dx, strict byte 00026h                ; 83 c2 26
    mov ax, word [bp+014h]                    ; 8b 46 14
    xor bx, bx                                ; 31 db
    call 0167ah                               ; e8 e7 b4
    mov ax, ss                                ; 8c d0
    mov cx, ax                                ; 89 c1
    sal cx, 004h                              ; c1 e1 04
    shr ax, 00ch                              ; c1 e8 0c
    mov word [bp-008h], ax                    ; 89 46 f8
    mov dx, word [bp+006h]                    ; 8b 56 06
    add dx, strict byte 00028h                ; 83 c2 28
    mov ax, word [bp+014h]                    ; 8b 46 14
    mov bx, strict word 0ffffh                ; bb ff ff
    call 0167ah                               ; e8 cb b4
    mov dx, word [bp+006h]                    ; 8b 56 06
    add dx, strict byte 0002ah                ; 83 c2 2a
    mov ax, word [bp+014h]                    ; 8b 46 14
    mov bx, cx                                ; 89 cb
    call 0167ah                               ; e8 bd b4
    movzx bx, byte [bp-008h]                  ; 0f b6 5e f8
    mov dx, word [bp+006h]                    ; 8b 56 06
    add dx, strict byte 0002ch                ; 83 c2 2c
    mov ax, word [bp+014h]                    ; 8b 46 14
    call 0165eh                               ; e8 91 b4
    mov dx, word [bp+006h]                    ; 8b 56 06
    add dx, strict byte 0002dh                ; 83 c2 2d
    mov ax, word [bp+014h]                    ; 8b 46 14
    mov bx, 00093h                            ; bb 93 00
    call 0165eh                               ; e8 82 b4
    mov dx, word [bp+006h]                    ; 8b 56 06
    add dx, strict byte 0002eh                ; 83 c2 2e
    mov ax, word [bp+014h]                    ; 8b 46 14
    xor bx, bx                                ; 31 db
    call 0167ah                               ; e8 90 b4
    mov si, word [bp+006h]                    ; 8b 76 06
    mov es, [bp+014h]                         ; 8e 46 14
    mov cx, word [bp+010h]                    ; 8b 4e 10
    push DS                                   ; 1e
    push eax                                  ; 66 50
    db  066h, 033h, 0c0h
    ; xor eax, eax                              ; 66 33 c0
    mov ds, ax                                ; 8e d8
    mov word [00467h], sp                     ; 89 26 67 04
    mov [00469h], ss                          ; 8c 16 69 04
    call 06206h                               ; e8 00 00
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
    call 0623ah                               ; e8 00 00
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
    call 05e89h                               ; e8 1e fc
    sti                                       ; fb
    mov byte [bp+013h], 000h                  ; c6 46 13 00
    and byte [bp+018h], 0feh                  ; 80 66 18 fe
    jmp near 06021h                           ; e9 aa fd
    mov ax, strict word 00031h                ; b8 31 00
    call 016ach                               ; e8 2f b4
    xor ah, ah                                ; 30 e4
    mov dx, ax                                ; 89 c2
    sal dx, 008h                              ; c1 e2 08
    mov ax, strict word 00030h                ; b8 30 00
    call 016ach                               ; e8 22 b4
    xor ah, ah                                ; 30 e4
    or dx, ax                                 ; 09 c2
    mov word [bp+012h], dx                    ; 89 56 12
    cmp dx, strict byte 0ffc0h                ; 83 fa c0
    jbe short 06270h                          ; 76 da
    mov word [bp+012h], strict word 0ffc0h    ; c7 46 12 c0 ff
    jmp short 06270h                          ; eb d3
    cli                                       ; fa
    mov ax, strict word 00001h                ; b8 01 00
    call 05e89h                               ; e8 e5 fb
    mov dx, word [bp+006h]                    ; 8b 56 06
    add dx, strict byte 00038h                ; 83 c2 38
    mov ax, word [bp+014h]                    ; 8b 46 14
    mov bx, strict word 0ffffh                ; bb ff ff
    call 0167ah                               ; e8 c7 b3
    mov dx, word [bp+006h]                    ; 8b 56 06
    add dx, strict byte 0003ah                ; 83 c2 3a
    mov ax, word [bp+014h]                    ; 8b 46 14
    xor bx, bx                                ; 31 db
    call 0167ah                               ; e8 b9 b3
    mov dx, word [bp+006h]                    ; 8b 56 06
    add dx, strict byte 0003ch                ; 83 c2 3c
    mov ax, word [bp+014h]                    ; 8b 46 14
    mov bx, strict word 0000fh                ; bb 0f 00
    call 0165eh                               ; e8 8e b3
    mov dx, word [bp+006h]                    ; 8b 56 06
    add dx, strict byte 0003dh                ; 83 c2 3d
    mov ax, word [bp+014h]                    ; 8b 46 14
    mov bx, 0009bh                            ; bb 9b 00
    call 0165eh                               ; e8 7f b3
    mov dx, word [bp+006h]                    ; 8b 56 06
    add dx, strict byte 0003eh                ; 83 c2 3e
    mov ax, word [bp+014h]                    ; 8b 46 14
    xor bx, bx                                ; 31 db
    call 0167ah                               ; e8 8d b3
    mov AL, strict byte 011h                  ; b0 11
    mov dx, strict word 00020h                ; ba 20 00
    out DX, AL                                ; ee
    mov dx, 000a0h                            ; ba a0 00
    out DX, AL                                ; ee
    mov ax, word [bp+00ch]                    ; 8b 46 0c
    shr ax, 008h                              ; c1 e8 08
    mov dx, strict word 00021h                ; ba 21 00
    out DX, AL                                ; ee
    mov ax, word [bp+00ch]                    ; 8b 46 0c
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
    mov si, word [bp+006h]                    ; 8b 76 06
    call 0632eh                               ; e8 00 00
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
    lea ax, [bp+004h]                         ; 8d 46 04
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
    jmp near 06021h                           ; e9 b7 fc
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 bc b5
    push 00876h                               ; 68 76 08
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 f5 b5
    add sp, strict byte 00004h                ; 83 c4 04
    or byte [bp+018h], 001h                   ; 80 4e 18 01
    mov ax, word [bp+012h]                    ; 8b 46 12
    xor ah, ah                                ; 30 e4
    or ah, 086h                               ; 80 cc 86
    mov word [bp+012h], ax                    ; 89 46 12
    jmp near 06021h                           ; e9 8f fc
    mov word [bp+018h], cx                    ; 89 4e 18
    mov word [bp+012h], ax                    ; 89 46 12
    mov word [bp+00ch], 0e6f5h                ; c7 46 0c f5 e6
    mov word [bp+014h], 0f000h                ; c7 46 14 00 f0
    jmp near 06021h                           ; e9 7c fc
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 be b2
    mov word [bp+014h], ax                    ; 89 46 14
    jmp near 06270h                           ; e9 bc fe
    push 008a5h                               ; 68 a5 08
    push strict byte 00008h                   ; 6a 08
    jmp short 0637ah                          ; eb bf
    test byte [bp+012h], 0ffh                 ; f6 46 12 ff
    jne short 063e0h                          ; 75 1f
    mov word [bp+012h], ax                    ; 89 46 12
    mov ax, word [bp+00ch]                    ; 8b 46 0c
    xor ah, ah                                ; 30 e4
    cmp ax, strict word 00001h                ; 3d 01 00
    jc short 063d9h                           ; 72 0b
    cmp ax, strict word 00003h                ; 3d 03 00
    jnbe short 063d9h                         ; 77 06
    mov word [bp+018h], cx                    ; 89 4e 18
    jmp near 06021h                           ; e9 48 fc
    or byte [bp+018h], 001h                   ; 80 4e 18 01
    jmp near 06021h                           ; e9 41 fc
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 46 b5
    push word [bp+00ch]                       ; ff 76 0c
    push word [bp+012h]                       ; ff 76 12
    push 008bch                               ; 68 bc 08
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 79 b5
    add sp, strict byte 00008h                ; 83 c4 08
    jmp short 06380h                          ; eb 82
    mov BH, strict byte 065h                  ; b7 65
    fldenv [di-004h]                          ; d9 65 fc
    db  065h, 01eh
    ; gs push DS                                ; 65 1e
    db  066h, 03eh, 066h, 05fh
    ; ds pop edi                                ; 66 3e 66 5f
    db  066h, 09eh, 066h, 0cah, 066h
_int15_function32:                           ; 0xf640e LB 0x37e
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    sub sp, strict byte 00008h                ; 83 ec 08
    mov dx, word [bp+020h]                    ; 8b 56 20
    shr dx, 008h                              ; c1 ea 08
    mov bx, word [bp+028h]                    ; 8b 5e 28
    and bl, 0feh                              ; 80 e3 fe
    mov ax, word [bp+020h]                    ; 8b 46 20
    xor ah, ah                                ; 30 e4
    cmp dx, 000e8h                            ; 81 fa e8 00
    je near 064dbh                            ; 0f 84 ad 00
    cmp dx, 000d0h                            ; 81 fa d0 00
    je short 06476h                           ; 74 42
    cmp dx, 00086h                            ; 81 fa 86 00
    jne near 0675ch                           ; 0f 85 20 03
    sti                                       ; fb
    mov ax, word [bp+01ch]                    ; 8b 46 1c
    mov dx, word [bp+018h]                    ; 8b 56 18
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
    je near 06473h                            ; 0f 84 0e 00
    in AL, strict byte 061h                   ; e4 61
    and AL, strict byte 010h                  ; 24 10
    db  03ah, 0c4h
    ; cmp al, ah                                ; 3a c4
    je short 06465h                           ; 74 f8
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0
    dec ecx                                   ; 66 49
    jne short 06465h                          ; 75 f2
    jmp near 06786h                           ; e9 10 03
    cmp ax, strict word 0004fh                ; 3d 4f 00
    jne near 0675ch                           ; 0f 85 df 02
    cmp word [bp+016h], 05052h                ; 81 7e 16 52 50
    jne near 0675ch                           ; 0f 85 d6 02
    cmp word [bp+014h], 04f43h                ; 81 7e 14 43 4f
    jne near 0675ch                           ; 0f 85 cd 02
    cmp word [bp+01eh], 04d4fh                ; 81 7e 1e 4f 4d
    jne near 0675ch                           ; 0f 85 c4 02
    cmp word [bp+01ch], 04445h                ; 81 7e 1c 45 44
    jne near 0675ch                           ; 0f 85 bb 02
    mov ax, word [bp+00ah]                    ; 8b 46 0a
    or ax, word [bp+008h]                     ; 0b 46 08
    jne near 0675ch                           ; 0f 85 b1 02
    mov ax, word [bp+006h]                    ; 8b 46 06
    or ax, word [bp+004h]                     ; 0b 46 04
    jne near 0675ch                           ; 0f 85 a7 02
    mov word [bp+028h], bx                    ; 89 5e 28
    mov ax, word [bp+014h]                    ; 8b 46 14
    mov word [bp+008h], ax                    ; 89 46 08
    mov ax, word [bp+016h]                    ; 8b 46 16
    mov word [bp+00ah], ax                    ; 89 46 0a
    mov ax, word [bp+01ch]                    ; 8b 46 1c
    mov word [bp+004h], ax                    ; 89 46 04
    mov ax, word [bp+01eh]                    ; 8b 46 1e
    mov word [bp+006h], ax                    ; 89 46 06
    mov dword [bp+020h], strict dword 049413332h ; 66 c7 46 20 32 33 41 49
    jmp near 06786h                           ; e9 ab 02
    cmp ax, strict word 00020h                ; 3d 20 00
    je short 064eah                           ; 74 0a
    cmp ax, strict word 00001h                ; 3d 01 00
    je near 06711h                            ; 0f 84 2a 02
    jmp near 0675ch                           ; e9 72 02
    cmp word [bp+01ah], 0534dh                ; 81 7e 1a 4d 53
    jne near 0675ch                           ; 0f 85 69 02
    cmp word [bp+018h], 04150h                ; 81 7e 18 50 41
    jne near 0675ch                           ; 0f 85 60 02
    mov ax, strict word 00035h                ; b8 35 00
    call 016ach                               ; e8 aa b1
    movzx bx, al                              ; 0f b6 d8
    xor dx, dx                                ; 31 d2
    mov cx, strict word 00008h                ; b9 08 00
    sal bx, 1                                 ; d1 e3
    rcl dx, 1                                 ; d1 d2
    loop 0650ah                               ; e2 fa
    mov ax, strict word 00034h                ; b8 34 00
    call 016ach                               ; e8 96 b1
    xor ah, ah                                ; 30 e4
    mov dx, bx                                ; 89 da
    or dx, ax                                 ; 09 c2
    xor bx, bx                                ; 31 db
    add bx, bx                                ; 01 db
    adc dx, 00100h                            ; 81 d2 00 01
    cmp dx, 00100h                            ; 81 fa 00 01
    jc short 06530h                           ; 72 06
    jne short 0655dh                          ; 75 31
    test bx, bx                               ; 85 db
    jnbe short 0655dh                         ; 77 2d
    mov ax, strict word 00031h                ; b8 31 00
    call 016ach                               ; e8 76 b1
    movzx bx, al                              ; 0f b6 d8
    xor dx, dx                                ; 31 d2
    mov cx, strict word 00008h                ; b9 08 00
    sal bx, 1                                 ; d1 e3
    rcl dx, 1                                 ; d1 d2
    loop 0653eh                               ; e2 fa
    mov ax, strict word 00030h                ; b8 30 00
    call 016ach                               ; e8 62 b1
    xor ah, ah                                ; 30 e4
    or bx, ax                                 ; 09 c3
    mov cx, strict word 0000ah                ; b9 0a 00
    sal bx, 1                                 ; d1 e3
    rcl dx, 1                                 ; d1 d2
    loop 06551h                               ; e2 fa
    add bx, strict byte 00000h                ; 83 c3 00
    adc dx, strict byte 00010h                ; 83 d2 10
    mov ax, strict word 00062h                ; b8 62 00
    call 016ach                               ; e8 49 b1
    xor ah, ah                                ; 30 e4
    mov word [bp-00ah], ax                    ; 89 46 f6
    xor al, al                                ; 30 c0
    mov word [bp-008h], ax                    ; 89 46 f8
    mov cx, strict word 00008h                ; b9 08 00
    sal word [bp-00ah], 1                     ; d1 66 f6
    rcl word [bp-008h], 1                     ; d1 56 f8
    loop 06570h                               ; e2 f8
    mov ax, strict word 00061h                ; b8 61 00
    call 016ach                               ; e8 2e b1
    xor ah, ah                                ; 30 e4
    or word [bp-00ah], ax                     ; 09 46 f6
    mov ax, word [bp-00ah]                    ; 8b 46 f6
    mov word [bp-008h], ax                    ; 89 46 f8
    mov word [bp-00ah], strict word 00000h    ; c7 46 f6 00 00
    mov ax, strict word 00063h                ; b8 63 00
    call 016ach                               ; e8 18 b1
    mov byte [bp-004h], al                    ; 88 46 fc
    mov byte [bp-006h], al                    ; 88 46 fa
    mov ax, word [bp+014h]                    ; 8b 46 14
    cmp ax, strict word 00007h                ; 3d 07 00
    jnbe near 0675ch                          ; 0f 87 b8 01
    mov si, ax                                ; 89 c6
    add si, ax                                ; 01 c6
    mov cx, bx                                ; 89 d9
    add cx, strict byte 00000h                ; 83 c1 00
    mov ax, dx                                ; 89 d0
    adc ax, strict word 0ffffh                ; 15 ff ff
    jmp word [cs:si+063feh]                   ; 2e ff a4 fe 63
    push strict byte 00001h                   ; 6a 01
    push dword 000000000h                     ; 66 6a 00
    push strict byte 00009h                   ; 6a 09
    push 0fc00h                               ; 68 00 fc
    mov dx, word [bp+004h]                    ; 8b 56 04
    mov ax, word [bp+024h]                    ; 8b 46 24
    xor bx, bx                                ; 31 db
    xor cx, cx                                ; 31 c9
    call 05eb5h                               ; e8 e7 f8
    mov dword [bp+014h], strict dword 000000001h ; 66 c7 46 14 01 00 00 00
    jmp near 066fbh                           ; e9 22 01
    push strict byte 00002h                   ; 6a 02
    push dword 000000000h                     ; 66 6a 00
    push strict byte 0000ah                   ; 6a 0a
    push strict byte 00000h                   ; 6a 00
    mov dx, word [bp+004h]                    ; 8b 56 04
    mov ax, word [bp+024h]                    ; 8b 46 24
    mov bx, 0fc00h                            ; bb 00 fc
    mov cx, strict word 00009h                ; b9 09 00
    call 05eb5h                               ; e8 c4 f8
    mov dword [bp+014h], strict dword 000000002h ; 66 c7 46 14 02 00 00 00
    jmp near 066fbh                           ; e9 ff 00
    push strict byte 00002h                   ; 6a 02
    push dword 000000000h                     ; 66 6a 00
    push strict byte 00010h                   ; 6a 10
    push strict byte 00000h                   ; 6a 00
    mov dx, word [bp+004h]                    ; 8b 56 04
    mov ax, word [bp+024h]                    ; 8b 46 24
    xor bx, bx                                ; 31 db
    mov cx, strict word 0000fh                ; b9 0f 00
    call 05eb5h                               ; e8 a2 f8
    mov dword [bp+014h], strict dword 000000003h ; 66 c7 46 14 03 00 00 00
    jmp near 066fbh                           ; e9 dd 00
    push strict byte 00001h                   ; 6a 01
    push dword 000000000h                     ; 66 6a 00
    push ax                                   ; 50
    push cx                                   ; 51
    mov dx, word [bp+004h]                    ; 8b 56 04
    mov ax, word [bp+024h]                    ; 8b 46 24
    xor bx, bx                                ; 31 db
    mov cx, strict word 00010h                ; b9 10 00
    call 05eb5h                               ; e8 82 f8
    mov dword [bp+014h], strict dword 000000004h ; 66 c7 46 14 04 00 00 00
    jmp near 066fbh                           ; e9 bd 00
    push strict byte 00003h                   ; 6a 03
    push dword 000000000h                     ; 66 6a 00
    push dx                                   ; 52
    push bx                                   ; 53
    mov dx, word [bp+004h]                    ; 8b 56 04
    mov si, word [bp+024h]                    ; 8b 76 24
    mov bx, cx                                ; 89 cb
    mov cx, ax                                ; 89 c1
    mov ax, si                                ; 89 f0
    call 05eb5h                               ; e8 61 f8
    mov dword [bp+014h], strict dword 000000005h ; 66 c7 46 14 05 00 00 00
    jmp near 066fbh                           ; e9 9c 00
    push strict byte 00002h                   ; 6a 02
    push dword 000000000h                     ; 66 6a 00
    push dword 000000000h                     ; 66 6a 00
    mov dx, word [bp+004h]                    ; 8b 56 04
    mov ax, word [bp+024h]                    ; 8b 46 24
    xor bx, bx                                ; 31 db
    mov cx, strict word 0fffch                ; b9 fc ff
    call 05eb5h                               ; e8 40 f8
    cmp byte [bp-004h], 000h                  ; 80 7e fc 00
    jne short 06682h                          ; 75 07
    mov ax, word [bp-008h]                    ; 8b 46 f8
    test ax, ax                               ; 85 c0
    je short 06696h                           ; 74 14
    mov dword [bp+014h], strict dword 000000007h ; 66 c7 46 14 07 00 00 00
    jmp short 066fbh                          ; eb 6f
    mov dword [bp+014h], strict dword 000000006h ; 66 c7 46 14 06 00 00 00
    jmp short 066fbh                          ; eb 65
    mov word [bp+014h], ax                    ; 89 46 14
    mov word [bp+016h], ax                    ; 89 46 16
    jmp short 066fbh                          ; eb 5d
    push strict byte 00002h                   ; 6a 02
    push dword 000000000h                     ; 66 6a 00
    push dword 000000000h                     ; 66 6a 00
    mov dx, word [bp+004h]                    ; 8b 56 04
    mov ax, word [bp+024h]                    ; 8b 46 24
    xor bx, bx                                ; 31 db
    xor cx, cx                                ; 31 c9
    call 05eb5h                               ; e8 02 f8
    cmp byte [bp-004h], 000h                  ; 80 7e fc 00
    jne short 066c0h                          ; 75 07
    mov ax, word [bp-008h]                    ; 8b 46 f8
    test ax, ax                               ; 85 c0
    je short 066c2h                           ; 74 02
    jmp short 06682h                          ; eb c0
    mov word [bp+014h], ax                    ; 89 46 14
    mov word [bp+016h], ax                    ; 89 46 16
    jmp short 066fbh                          ; eb 31
    cmp byte [bp-004h], 000h                  ; 80 7e fc 00
    jne short 066d6h                          ; 75 06
    cmp word [bp-008h], strict byte 00000h    ; 83 7e f8 00
    je short 066fbh                           ; 74 25
    push strict byte 00001h                   ; 6a 01
    mov al, byte [bp-006h]                    ; 8a 46 fa
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    push strict byte 00001h                   ; 6a 01
    push dword [bp-00ah]                      ; 66 ff 76 f6
    mov dx, word [bp+004h]                    ; 8b 56 04
    mov ax, word [bp+024h]                    ; 8b 46 24
    xor bx, bx                                ; 31 db
    xor cx, cx                                ; 31 c9
    call 05eb5h                               ; e8 c2 f7
    xor ax, ax                                ; 31 c0
    mov word [bp+014h], ax                    ; 89 46 14
    mov word [bp+016h], ax                    ; 89 46 16
    mov dword [bp+020h], strict dword 0534d4150h ; 66 c7 46 20 50 41 4d 53
    mov dword [bp+01ch], strict dword 000000014h ; 66 c7 46 1c 14 00 00 00
    and byte [bp+028h], 0feh                  ; 80 66 28 fe
    jmp short 06786h                          ; eb 75
    mov word [bp+028h], bx                    ; 89 5e 28
    mov ax, strict word 00031h                ; b8 31 00
    call 016ach                               ; e8 92 af
    movzx dx, al                              ; 0f b6 d0
    sal dx, 008h                              ; c1 e2 08
    mov ax, strict word 00030h                ; b8 30 00
    call 016ach                               ; e8 86 af
    xor ah, ah                                ; 30 e4
    or dx, ax                                 ; 09 c2
    mov word [bp+01ch], dx                    ; 89 56 1c
    cmp dx, 03c00h                            ; 81 fa 00 3c
    jbe short 06738h                          ; 76 05
    mov word [bp+01ch], 03c00h                ; c7 46 1c 00 3c
    mov ax, strict word 00035h                ; b8 35 00
    call 016ach                               ; e8 6e af
    movzx dx, al                              ; 0f b6 d0
    sal dx, 008h                              ; c1 e2 08
    mov ax, strict word 00034h                ; b8 34 00
    call 016ach                               ; e8 62 af
    xor ah, ah                                ; 30 e4
    or dx, ax                                 ; 09 c2
    mov word [bp+018h], dx                    ; 89 56 18
    mov ax, word [bp+01ch]                    ; 8b 46 1c
    mov word [bp+020h], ax                    ; 89 46 20
    mov word [bp+014h], dx                    ; 89 56 14
    jmp short 06786h                          ; eb 2a
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 ca b1
    push word [bp+014h]                       ; ff 76 14
    push word [bp+020h]                       ; ff 76 20
    push 008bch                               ; 68 bc 08
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 fd b1
    add sp, strict byte 00008h                ; 83 c4 08
    or byte [bp+028h], 001h                   ; 80 4e 28 01
    mov ax, word [bp+020h]                    ; 8b 46 20
    xor al, al                                ; 30 c0
    or AL, strict byte 086h                   ; 0c 86
    mov word [bp+020h], ax                    ; 89 46 20
    lea sp, [bp-002h]                         ; 8d 66 fe
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn                                      ; c3
_inv_op_handler:                             ; 0xf678c LB 0x195
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    push di                                   ; 57
    push ax                                   ; 50
    push ax                                   ; 50
    les bx, [bp+018h]                         ; c4 5e 18
    cmp byte [es:bx], 0f0h                    ; 26 80 3f f0
    jne short 067a2h                          ; 75 06
    inc word [bp+018h]                        ; ff 46 18
    jmp near 0691ah                           ; e9 78 01
    cmp word [es:bx], 0050fh                  ; 26 81 3f 0f 05
    jne near 06916h                           ; 0f 85 6b 01
    mov si, 00800h                            ; be 00 08
    xor ax, ax                                ; 31 c0
    mov word [bp-006h], ax                    ; 89 46 fa
    mov word [bp-008h], ax                    ; 89 46 f8
    mov es, ax                                ; 8e c0
    mov bx, word [es:si+02ch]                 ; 26 8b 5c 2c
    sub bx, strict byte 00006h                ; 83 eb 06
    mov dx, word [es:si+020h]                 ; 26 8b 54 20
    mov ax, word [es:si+01ah]                 ; 26 8b 44 1a
    mov es, dx                                ; 8e c2
    mov word [es:bx], ax                      ; 26 89 07
    mov es, [bp-006h]                         ; 8e 46 fa
    mov ax, word [es:si+022h]                 ; 26 8b 44 22
    mov es, dx                                ; 8e c2
    mov word [es:bx+002h], ax                 ; 26 89 47 02
    mov es, [bp-006h]                         ; 8e 46 fa
    mov ax, word [es:si+018h]                 ; 26 8b 44 18
    mov es, dx                                ; 8e c2
    mov word [es:bx+004h], ax                 ; 26 89 47 04
    mov es, [bp-006h]                         ; 8e 46 fa
    movzx bx, byte [es:si+038h]               ; 26 0f b6 5c 38
    mov di, word [es:si+036h]                 ; 26 8b 7c 36
    mov ax, word [es:si+024h]                 ; 26 8b 44 24
    xor dx, dx                                ; 31 d2
    mov cx, strict word 00004h                ; b9 04 00
    sal ax, 1                                 ; d1 e0
    rcl dx, 1                                 ; d1 d2
    loop 067fbh                               ; e2 fa
    cmp bx, dx                                ; 39 d3
    jne short 06809h                          ; 75 04
    cmp di, ax                                ; 39 c7
    je short 0680eh                           ; 74 05
    mov word [bp-008h], strict word 00001h    ; c7 46 f8 01 00
    mov es, [bp-006h]                         ; 8e 46 fa
    movzx di, byte [es:si+04ah]               ; 26 0f b6 7c 4a
    mov bx, word [es:si+048h]                 ; 26 8b 5c 48
    mov ax, word [es:si+01eh]                 ; 26 8b 44 1e
    xor dx, dx                                ; 31 d2
    mov cx, strict word 00004h                ; b9 04 00
    sal ax, 1                                 ; d1 e0
    rcl dx, 1                                 ; d1 d2
    loop 06823h                               ; e2 fa
    cmp di, dx                                ; 39 d7
    jne short 06831h                          ; 75 04
    cmp bx, ax                                ; 39 c3
    je short 06835h                           ; 74 04
    or byte [bp-008h], 002h                   ; 80 4e f8 02
    push strict byte 00000h                   ; 6a 00
    push 00800h                               ; 68 00 08
    push strict byte 0001fh                   ; 6a 1f
    db  08bh, 0dch
    ; mov bx, sp                                ; 8b dc
    lgdt [ss:bx]                              ; 36 0f 01 17
    add sp, strict byte 00006h                ; 83 c4 06
    mov es, [bp-006h]                         ; 8e 46 fa
    mov ax, word [es:si+03ah]                 ; 26 8b 44 3a
    mov word [es:si+008h], ax                 ; 26 89 44 08
    mov ax, word [es:si+036h]                 ; 26 8b 44 36
    mov word [es:si+00ah], ax                 ; 26 89 44 0a
    movzx dx, byte [es:si+039h]               ; 26 0f b6 54 39
    sal dx, 008h                              ; c1 e2 08
    movzx ax, byte [es:si+038h]               ; 26 0f b6 44 38
    or dx, ax                                 ; 09 c2
    mov word [es:si+00ch], dx                 ; 26 89 54 0c
    mov word [es:si+00eh], strict word 00000h ; 26 c7 44 0e 00 00
    mov ax, word [es:si+04ch]                 ; 26 8b 44 4c
    mov word [es:si], ax                      ; 26 89 04
    mov ax, word [es:si+048h]                 ; 26 8b 44 48
    mov word [es:si+002h], ax                 ; 26 89 44 02
    movzx dx, byte [es:si+04bh]               ; 26 0f b6 54 4b
    sal dx, 008h                              ; c1 e2 08
    movzx ax, byte [es:si+04ah]               ; 26 0f b6 44 4a
    or dx, ax                                 ; 09 c2
    mov word [es:si+004h], dx                 ; 26 89 54 04
    movzx ax, byte [es:si+05ch]               ; 26 0f b6 44 5c
    mov dx, word [es:si+05ah]                 ; 26 8b 54 5a
    push ax                                   ; 50
    push dx                                   ; 52
    push word [es:si+05eh]                    ; 26 ff 74 5e
    db  08bh, 0dch
    ; mov bx, sp                                ; 8b dc
    lidt [ss:bx]                              ; 36 0f 01 1f
    add sp, strict byte 00006h                ; 83 c4 06
    mov cx, word [bp-008h]                    ; 8b 4e f8
    mov ax, 00080h                            ; b8 80 00
    mov ss, ax                                ; 8e d0
    mov ax, word [ss:0001eh]                  ; 36 a1 1e 00
    mov ds, ax                                ; 8e d8
    mov ax, word [ss:00024h]                  ; 36 a1 24 00
    mov es, ax                                ; 8e c0
    smsw ax                                   ; 0f 01 e0
    inc ax                                    ; 40
    lmsw ax                                   ; 0f 01 f0
    mov ax, strict word 00008h                ; b8 08 00
    test cx, strict word 00001h               ; f7 c1 01 00
    je near 068d3h                            ; 0f 84 02 00
    mov es, ax                                ; 8e c0
    test cx, strict word 00002h               ; f7 c1 02 00
    je near 068fbh                            ; 0f 84 20 00
    mov bx, word [word ss:00000h]             ; 36 8b 1e 00 00
    mov word [word ss:00008h], bx             ; 36 89 1e 08 00
    mov bx, word [word ss:00002h]             ; 36 8b 1e 02 00
    mov word [word ss:0000ah], bx             ; 36 89 1e 0a 00
    mov bx, word [word ss:00004h]             ; 36 8b 1e 04 00
    mov word [word ss:0000ch], bx             ; 36 89 1e 0c 00
    mov ds, ax                                ; 8e d8
    mov eax, cr0                              ; 0f 20 c0
    dec ax                                    ; 48
    mov cr0, eax                              ; 0f 22 c0
    mov sp, strict word 00026h                ; bc 26 00
    popaw                                     ; 61
    mov sp, word [word ss:0002ch]             ; 36 8b 26 2c 00
    sub sp, strict byte 00006h                ; 83 ec 06
    mov ss, [word ss:00020h]                  ; 36 8e 16 20 00
    iret                                      ; cf
    jmp short 0691ah                          ; eb 04
    sti                                       ; fb
    hlt                                       ; f4
    jmp short 06917h                          ; eb fd
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn                                      ; c3
init_rtc_:                                   ; 0xf6921 LB 0x28
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push dx                                   ; 52
    mov dx, strict word 00026h                ; ba 26 00
    mov ax, strict word 0000ah                ; b8 0a 00
    call 016c9h                               ; e8 9b ad
    mov dx, strict word 00002h                ; ba 02 00
    mov ax, strict word 0000bh                ; b8 0b 00
    call 016c9h                               ; e8 92 ad
    mov ax, strict word 0000ch                ; b8 0c 00
    call 016ach                               ; e8 6f ad
    mov ax, strict word 0000dh                ; b8 0d 00
    call 016ach                               ; e8 69 ad
    lea sp, [bp-002h]                         ; 8d 66 fe
    pop dx                                    ; 5a
    pop bp                                    ; 5d
    retn                                      ; c3
rtc_updating_:                               ; 0xf6949 LB 0x21
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push dx                                   ; 52
    mov dx, 061a8h                            ; ba a8 61
    dec dx                                    ; 4a
    je short 06961h                           ; 74 0e
    mov ax, strict word 0000ah                ; b8 0a 00
    call 016ach                               ; e8 53 ad
    test AL, strict byte 080h                 ; a8 80
    jne short 06950h                          ; 75 f3
    xor ax, ax                                ; 31 c0
    jmp short 06964h                          ; eb 03
    mov ax, strict word 00001h                ; b8 01 00
    lea sp, [bp-002h]                         ; 8d 66 fe
    pop dx                                    ; 5a
    pop bp                                    ; 5d
    retn                                      ; c3
_int70_function:                             ; 0xf696a LB 0xbe
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    push ax                                   ; 50
    mov ax, strict word 0000bh                ; b8 0b 00
    call 016ach                               ; e8 37 ad
    mov dl, al                                ; 88 c2
    mov byte [bp-004h], al                    ; 88 46 fc
    mov ax, strict word 0000ch                ; b8 0c 00
    call 016ach                               ; e8 2c ad
    mov dh, al                                ; 88 c6
    test dl, 060h                             ; f6 c2 60
    je near 06a0fh                            ; 0f 84 86 00
    test AL, strict byte 020h                 ; a8 20
    je short 06991h                           ; 74 04
    sti                                       ; fb
    int 04ah                                  ; cd 4a
    cli                                       ; fa
    test dh, 040h                             ; f6 c6 40
    je near 06a0fh                            ; 0f 84 77 00
    mov dx, 000a0h                            ; ba a0 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01650h                               ; e8 af ac
    test al, al                               ; 84 c0
    je short 06a0fh                           ; 74 6a
    mov dx, 0009ch                            ; ba 9c 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01688h                               ; e8 da ac
    test dx, dx                               ; 85 d2
    jne short 069fbh                          ; 75 49
    cmp ax, 003d1h                            ; 3d d1 03
    jnc short 069fbh                          ; 73 44
    mov dx, 00098h                            ; ba 98 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 ac ac
    mov si, ax                                ; 89 c6
    mov dx, 0009ah                            ; ba 9a 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 a1 ac
    mov cx, ax                                ; 89 c1
    xor bx, bx                                ; 31 db
    mov dx, 000a0h                            ; ba a0 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 86 ac
    mov al, byte [bp-004h]                    ; 8a 46 fc
    and AL, strict byte 037h                  ; 24 37
    movzx dx, al                              ; 0f b6 d0
    mov ax, strict word 0000bh                ; b8 0b 00
    call 016c9h                               ; e8 e3 ac
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 01650h                               ; e8 63 ac
    or AL, strict byte 080h                   ; 0c 80
    movzx bx, al                              ; 0f b6 d8
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 0165eh                               ; e8 65 ac
    jmp short 06a0fh                          ; eb 14
    mov bx, ax                                ; 89 c3
    add bx, 0fc2fh                            ; 81 c3 2f fc
    mov cx, dx                                ; 89 d1
    adc cx, strict byte 0ffffh                ; 83 d1 ff
    mov dx, 0009ch                            ; ba 9c 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0169ah                               ; e8 8b ac
    call 0e03bh                               ; e8 29 76
    lea sp, [bp-002h]                         ; 8d 66 fe
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn                                      ; c3
    aas                                       ; 3f
    push strict byte 00068h                   ; 6a 68
    push strict byte 0ff8dh                   ; 6a 8d
    push strict byte 0ffbfh                   ; 6a bf
    push strict byte 0000eh                   ; 6a 0e
    imul ax, word [bp+06bh], strict byte 0ff89h ; 6b 46 6b 89
    imul sp, ax, strict byte 0006bh           ; 6b e0 6b
_int1a_function:                             ; 0xf6a28 LB 0x1c8
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sti                                       ; fb
    mov al, byte [bp+013h]                    ; 8a 46 13
    cmp AL, strict byte 007h                  ; 3c 07
    jnbe near 06a64h                          ; 0f 87 2f 00
    movzx bx, al                              ; 0f b6 d8
    add bx, bx                                ; 01 db
    jmp word [cs:bx+06a18h]                   ; 2e ff a7 18 6a
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
    mov sp, bp                                ; 89 ec
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
    jmp short 06a64h                          ; eb d7
    call 06949h                               ; e8 b9 fe
    test ax, ax                               ; 85 c0
    je short 06a96h                           ; 74 02
    jmp short 06a64h                          ; eb ce
    xor ax, ax                                ; 31 c0
    call 016ach                               ; e8 11 ac
    mov byte [bp+00fh], al                    ; 88 46 0f
    mov ax, strict word 00002h                ; b8 02 00
    call 016ach                               ; e8 08 ac
    mov byte [bp+010h], al                    ; 88 46 10
    mov ax, strict word 00004h                ; b8 04 00
    call 016ach                               ; e8 ff ab
    mov bl, al                                ; 88 c3
    mov byte [bp+011h], al                    ; 88 46 11
    mov ax, strict word 0000bh                ; b8 0b 00
    call 016ach                               ; e8 f4 ab
    and AL, strict byte 001h                  ; 24 01
    mov byte [bp+00eh], al                    ; 88 46 0e
    jmp short 06b04h                          ; eb 45
    call 06949h                               ; e8 87 fe
    test ax, ax                               ; 85 c0
    je short 06ac9h                           ; 74 03
    call 06921h                               ; e8 58 fe
    movzx dx, byte [bp+00fh]                  ; 0f b6 56 0f
    xor ax, ax                                ; 31 c0
    call 016c9h                               ; e8 f7 ab
    movzx dx, byte [bp+010h]                  ; 0f b6 56 10
    mov ax, strict word 00002h                ; b8 02 00
    call 016c9h                               ; e8 ed ab
    movzx dx, byte [bp+011h]                  ; 0f b6 56 11
    mov ax, strict word 00004h                ; b8 04 00
    call 016c9h                               ; e8 e3 ab
    mov ax, strict word 0000bh                ; b8 0b 00
    call 016ach                               ; e8 c0 ab
    mov bl, al                                ; 88 c3
    and bl, 060h                              ; 80 e3 60
    or bl, 002h                               ; 80 cb 02
    mov al, byte [bp+00eh]                    ; 8a 46 0e
    and AL, strict byte 001h                  ; 24 01
    or bl, al                                 ; 08 c3
    movzx dx, bl                              ; 0f b6 d3
    mov ax, strict word 0000bh                ; b8 0b 00
    call 016c9h                               ; e8 c5 ab
    mov byte [bp+013h], 000h                  ; c6 46 13 00
    mov byte [bp+012h], bl                    ; 88 5e 12
    jmp near 06a64h                           ; e9 56 ff
    mov byte [bp+013h], 000h                  ; c6 46 13 00
    call 06949h                               ; e8 34 fe
    test ax, ax                               ; 85 c0
    je short 06b1ch                           ; 74 03
    jmp near 06a64h                           ; e9 48 ff
    mov ax, strict word 00009h                ; b8 09 00
    call 016ach                               ; e8 8a ab
    mov byte [bp+010h], al                    ; 88 46 10
    mov ax, strict word 00008h                ; b8 08 00
    call 016ach                               ; e8 81 ab
    mov byte [bp+00fh], al                    ; 88 46 0f
    mov ax, strict word 00007h                ; b8 07 00
    call 016ach                               ; e8 78 ab
    mov byte [bp+00eh], al                    ; 88 46 0e
    mov ax, strict word 00032h                ; b8 32 00
    call 016ach                               ; e8 6f ab
    mov byte [bp+011h], al                    ; 88 46 11
    mov byte [bp+012h], al                    ; 88 46 12
    jmp near 06a64h                           ; e9 1e ff
    call 06949h                               ; e8 00 fe
    test ax, ax                               ; 85 c0
    je short 06b53h                           ; 74 06
    call 06921h                               ; e8 d1 fd
    jmp near 06a64h                           ; e9 11 ff
    movzx dx, byte [bp+010h]                  ; 0f b6 56 10
    mov ax, strict word 00009h                ; b8 09 00
    call 016c9h                               ; e8 6c ab
    movzx dx, byte [bp+00fh]                  ; 0f b6 56 0f
    mov ax, strict word 00008h                ; b8 08 00
    call 016c9h                               ; e8 62 ab
    movzx dx, byte [bp+00eh]                  ; 0f b6 56 0e
    mov ax, strict word 00007h                ; b8 07 00
    call 016c9h                               ; e8 58 ab
    movzx dx, byte [bp+011h]                  ; 0f b6 56 11
    mov ax, strict word 00032h                ; b8 32 00
    call 016c9h                               ; e8 4e ab
    mov ax, strict word 0000bh                ; b8 0b 00
    call 016ach                               ; e8 2b ab
    mov bl, al                                ; 88 c3
    and bl, 07fh                              ; 80 e3 7f
    jmp near 06afbh                           ; e9 72 ff
    mov ax, strict word 0000bh                ; b8 0b 00
    call 016ach                               ; e8 1d ab
    mov bl, al                                ; 88 c3
    mov word [bp+012h], strict word 00000h    ; c7 46 12 00 00
    test AL, strict byte 020h                 ; a8 20
    je short 06b9dh                           ; 74 03
    jmp near 06a64h                           ; e9 c7 fe
    call 06949h                               ; e8 a9 fd
    test ax, ax                               ; 85 c0
    je short 06ba7h                           ; 74 03
    call 06921h                               ; e8 7a fd
    movzx dx, byte [bp+00fh]                  ; 0f b6 56 0f
    mov ax, strict word 00001h                ; b8 01 00
    call 016c9h                               ; e8 18 ab
    movzx dx, byte [bp+010h]                  ; 0f b6 56 10
    mov ax, strict word 00003h                ; b8 03 00
    call 016c9h                               ; e8 0e ab
    movzx dx, byte [bp+011h]                  ; 0f b6 56 11
    mov ax, strict word 00005h                ; b8 05 00
    call 016c9h                               ; e8 04 ab
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
    call 016c9h                               ; e8 ec aa
    jmp near 06a64h                           ; e9 84 fe
    mov ax, strict word 0000bh                ; b8 0b 00
    call 016ach                               ; e8 c6 aa
    mov bl, al                                ; 88 c3
    and AL, strict byte 057h                  ; 24 57
    movzx dx, al                              ; 0f b6 d0
    jmp near 06afeh                           ; e9 0e ff
send_to_mouse_ctrl_:                         ; 0xf6bf0 LB 0x34
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    push dx                                   ; 52
    mov bl, al                                ; 88 c3
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 002h                 ; a8 02
    je short 06c0fh                           ; 74 0e
    push 008f6h                               ; 68 f6 08
    push 01152h                               ; 68 52 11
    push strict byte 00007h                   ; 6a 07
    call 01972h                               ; e8 66 ad
    add sp, strict byte 00006h                ; 83 c4 06
    mov AL, strict byte 0d4h                  ; b0 d4
    mov dx, strict word 00064h                ; ba 64 00
    out DX, AL                                ; ee
    mov al, bl                                ; 88 d8
    mov dx, strict word 00060h                ; ba 60 00
    out DX, AL                                ; ee
    xor al, bl                                ; 30 d8
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
get_mouse_data_:                             ; 0xf6c24 LB 0x3b
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    push cx                                   ; 51
    mov bx, ax                                ; 89 c3
    mov es, dx                                ; 8e c2
    mov cx, 02710h                            ; b9 10 27
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and ax, strict word 00021h                ; 25 21 00
    cmp ax, strict word 00021h                ; 3d 21 00
    je short 06c45h                           ; 74 07
    test cx, cx                               ; 85 c9
    je short 06c45h                           ; 74 03
    dec cx                                    ; 49
    jmp short 06c30h                          ; eb eb
    test cx, cx                               ; 85 c9
    jne short 06c4dh                          ; 75 04
    mov AL, strict byte 001h                  ; b0 01
    jmp short 06c58h                          ; eb 0b
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov byte [es:bx], al                      ; 26 88 07
    xor al, al                                ; 30 c0
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop cx                                    ; 59
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
set_kbd_command_byte_:                       ; 0xf6c5f LB 0x32
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    push dx                                   ; 52
    mov bl, al                                ; 88 c3
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 002h                 ; a8 02
    je short 06c7eh                           ; 74 0e
    push 00900h                               ; 68 00 09
    push 01152h                               ; 68 52 11
    push strict byte 00007h                   ; 6a 07
    call 01972h                               ; e8 f7 ac
    add sp, strict byte 00006h                ; 83 c4 06
    mov AL, strict byte 060h                  ; b0 60
    mov dx, strict word 00064h                ; ba 64 00
    out DX, AL                                ; ee
    mov al, bl                                ; 88 d8
    mov dx, strict word 00060h                ; ba 60 00
    out DX, AL                                ; ee
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
_int74_function:                             ; 0xf6c91 LB 0xca
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sub sp, strict byte 00008h                ; 83 ec 08
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 cc a9
    mov cx, ax                                ; 89 c1
    mov word [bp+004h], strict word 00000h    ; c7 46 04 00 00
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and AL, strict byte 021h                  ; 24 21
    cmp AL, strict byte 021h                  ; 3c 21
    jne near 06d47h                           ; 0f 85 92 00
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov bl, al                                ; 88 c3
    mov dx, strict word 00026h                ; ba 26 00
    mov ax, cx                                ; 89 c8
    call 01650h                               ; e8 8b a9
    mov byte [bp-006h], al                    ; 88 46 fa
    mov dx, strict word 00027h                ; ba 27 00
    mov ax, cx                                ; 89 c8
    call 01650h                               ; e8 80 a9
    mov byte [bp-008h], al                    ; 88 46 f8
    test AL, strict byte 080h                 ; a8 80
    je short 06d47h                           ; 74 70
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
    call 0165eh                               ; e8 6a a9
    mov al, byte [bp-004h]                    ; 8a 46 fc
    cmp al, byte [bp-002h]                    ; 3a 46 fe
    jc short 06d38h                           ; 72 3c
    mov dx, strict word 00028h                ; ba 28 00
    mov ax, cx                                ; 89 c8
    call 01650h                               ; e8 4c a9
    xor ah, ah                                ; 30 e4
    mov word [bp+00ch], ax                    ; 89 46 0c
    mov dx, strict word 00029h                ; ba 29 00
    mov ax, cx                                ; 89 c8
    call 01650h                               ; e8 3f a9
    xor ah, ah                                ; 30 e4
    mov word [bp+00ah], ax                    ; 89 46 0a
    mov dx, strict word 0002ah                ; ba 2a 00
    mov ax, cx                                ; 89 c8
    call 01650h                               ; e8 32 a9
    xor ah, ah                                ; 30 e4
    mov word [bp+008h], ax                    ; 89 46 08
    xor al, al                                ; 30 c0
    mov word [bp+006h], ax                    ; 89 46 06
    mov byte [bp-006h], ah                    ; 88 66 fa
    test byte [bp-008h], 080h                 ; f6 46 f8 80
    je short 06d3bh                           ; 74 0a
    mov word [bp+004h], strict word 00001h    ; c7 46 04 01 00
    jmp short 06d3bh                          ; eb 03
    inc byte [bp-006h]                        ; fe 46 fa
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa
    mov dx, strict word 00026h                ; ba 26 00
    mov ax, cx                                ; 89 c8
    call 0165eh                               ; e8 17 a9
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    retn                                      ; c3
    lahf                                      ; 9f
    insw                                      ; 6d
    adc ax, 0986eh                            ; 15 6e 98
    outsb                                     ; 6e
    sub word [bx-069h], bp                    ; 29 6f 97
    outsw                                     ; 6f
    jmp short 06dc4h                          ; eb 6d
    mov di, 0846fh                            ; bf 6f 84
    db  070h
_int15_function_mouse:                       ; 0xf6d5b LB 0x38b
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    sub sp, strict byte 00006h                ; 83 ec 06
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 01 a9
    mov cx, ax                                ; 89 c1
    cmp byte [bp+012h], 007h                  ; 80 7e 12 07
    jbe short 06d7eh                          ; 76 0b
    or word [bp+018h], strict byte 00001h     ; 83 4e 18 01
    mov byte [bp+013h], 001h                  ; c6 46 13 01
    jmp near 070e0h                           ; e9 62 03
    mov ax, strict word 00065h                ; b8 65 00
    call 06c5fh                               ; e8 db fe
    and word [bp+018h], strict byte 0fffeh    ; 83 66 18 fe
    mov byte [bp+013h], 000h                  ; c6 46 13 00
    mov al, byte [bp+012h]                    ; 8a 46 12
    cmp AL, strict byte 007h                  ; 3c 07
    jnbe near 070c7h                          ; 0f 87 32 03
    movzx si, al                              ; 0f b6 f0
    add si, si                                ; 01 f6
    jmp word [cs:si+06d4bh]                   ; 2e ff a4 4b 6d
    cmp byte [bp+00dh], 001h                  ; 80 7e 0d 01
    jnbe near 070d2h                          ; 0f 87 2b 03
    mov dx, strict word 00027h                ; ba 27 00
    mov ax, cx                                ; 89 c8
    call 01650h                               ; e8 a1 a8
    test AL, strict byte 080h                 ; a8 80
    jne short 06dbeh                          ; 75 0b
    or word [bp+018h], strict byte 00001h     ; 83 4e 18 01
    mov byte [bp+013h], 005h                  ; c6 46 13 05
    jmp near 070dah                           ; e9 1c 03
    cmp byte [bp+00dh], 000h                  ; 80 7e 0d 00
    db  00fh, 094h, 0c0h
    ; sete al                                   ; 0f 94 c0
    add AL, strict byte 0f4h                  ; 04 f4
    xor ah, ah                                ; 30 e4
    call 06bf0h                               ; e8 24 fe
    test al, al                               ; 84 c0
    jne near 07060h                           ; 0f 85 8e 02
    mov dx, ss                                ; 8c d2
    lea ax, [bp-008h]                         ; 8d 46 f8
    call 06c24h                               ; e8 4a fe
    test al, al                               ; 84 c0
    je near 070dah                            ; 0f 84 fa 02
    cmp byte [bp-008h], 0fah                  ; 80 7e f8 fa
    jne near 07060h                           ; 0f 85 78 02
    jmp near 070dah                           ; e9 ef 02
    mov al, byte [bp+00dh]                    ; 8a 46 0d
    cmp AL, strict byte 001h                  ; 3c 01
    jc short 06df6h                           ; 72 04
    cmp AL, strict byte 008h                  ; 3c 08
    jbe short 06df9h                          ; 76 03
    jmp near 06f8ch                           ; e9 93 01
    mov dx, strict word 00027h                ; ba 27 00
    mov ax, cx                                ; 89 c8
    call 01650h                               ; e8 4f a8
    mov ah, byte [bp+00dh]                    ; 8a 66 0d
    db  0feh, 0cch
    ; dec ah                                    ; fe cc
    and AL, strict byte 0f8h                  ; 24 f8
    or al, ah                                 ; 08 e0
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 00027h                ; ba 27 00
    mov ax, cx                                ; 89 c8
    call 0165eh                               ; e8 49 a8
    mov dx, strict word 00026h                ; ba 26 00
    mov ax, cx                                ; 89 c8
    call 01650h                               ; e8 33 a8
    and AL, strict byte 0f8h                  ; 24 f8
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 00026h                ; ba 26 00
    mov ax, cx                                ; 89 c8
    call 0165eh                               ; e8 34 a8
    mov ax, 000ffh                            ; b8 ff 00
    call 06bf0h                               ; e8 c0 fd
    test al, al                               ; 84 c0
    jne near 07060h                           ; 0f 85 2a 02
    mov dx, ss                                ; 8c d2
    lea ax, [bp-004h]                         ; 8d 46 fc
    call 06c24h                               ; e8 e6 fd
    mov cl, al                                ; 88 c1
    cmp byte [bp-004h], 0feh                  ; 80 7e fc fe
    jne short 06e51h                          ; 75 0b
    or word [bp+018h], strict byte 00001h     ; 83 4e 18 01
    mov byte [bp+013h], 004h                  ; c6 46 13 04
    jmp near 070dah                           ; e9 89 02
    cmp byte [bp-004h], 0fah                  ; 80 7e fc fa
    je short 06e67h                           ; 74 10
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc
    push ax                                   ; 50
    push 0090bh                               ; 68 0b 09
    push strict byte 00007h                   ; 6a 07
    call 01972h                               ; e8 0e ab
    add sp, strict byte 00006h                ; 83 c4 06
    test cl, cl                               ; 84 c9
    jne near 07060h                           ; 0f 85 f3 01
    mov dx, ss                                ; 8c d2
    lea ax, [bp-008h]                         ; 8d 46 f8
    call 06c24h                               ; e8 af fd
    test al, al                               ; 84 c0
    jne near 07060h                           ; 0f 85 e5 01
    mov dx, ss                                ; 8c d2
    lea ax, [bp-006h]                         ; 8d 46 fa
    call 06c24h                               ; e8 a1 fd
    test al, al                               ; 84 c0
    jne near 07060h                           ; 0f 85 d7 01
    mov al, byte [bp-008h]                    ; 8a 46 f8
    mov byte [bp+00ch], al                    ; 88 46 0c
    mov al, byte [bp-006h]                    ; 8a 46 fa
    mov byte [bp+00dh], al                    ; 88 46 0d
    jmp near 070dah                           ; e9 42 02
    mov al, byte [bp+00dh]                    ; 8a 46 0d
    cmp AL, strict byte 003h                  ; 3c 03
    jc short 06eafh                           ; 72 10
    jbe short 06ecdh                          ; 76 2c
    cmp AL, strict byte 006h                  ; 3c 06
    je short 06edfh                           ; 74 3a
    cmp AL, strict byte 005h                  ; 3c 05
    je short 06ed9h                           ; 74 30
    cmp AL, strict byte 004h                  ; 3c 04
    je short 06ed3h                           ; 74 26
    jmp short 06ee5h                          ; eb 36
    cmp AL, strict byte 002h                  ; 3c 02
    je short 06ec7h                           ; 74 14
    cmp AL, strict byte 001h                  ; 3c 01
    je short 06ec1h                           ; 74 0a
    test al, al                               ; 84 c0
    jne short 06ee5h                          ; 75 2a
    mov byte [bp-008h], 00ah                  ; c6 46 f8 0a
    jmp short 06ee9h                          ; eb 28
    mov byte [bp-008h], 014h                  ; c6 46 f8 14
    jmp short 06ee9h                          ; eb 22
    mov byte [bp-008h], 028h                  ; c6 46 f8 28
    jmp short 06ee9h                          ; eb 1c
    mov byte [bp-008h], 03ch                  ; c6 46 f8 3c
    jmp short 06ee9h                          ; eb 16
    mov byte [bp-008h], 050h                  ; c6 46 f8 50
    jmp short 06ee9h                          ; eb 10
    mov byte [bp-008h], 064h                  ; c6 46 f8 64
    jmp short 06ee9h                          ; eb 0a
    mov byte [bp-008h], 0c8h                  ; c6 46 f8 c8
    jmp short 06ee9h                          ; eb 04
    mov byte [bp-008h], 000h                  ; c6 46 f8 00
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00
    jbe short 06f1eh                          ; 76 2f
    mov ax, 000f3h                            ; b8 f3 00
    call 06bf0h                               ; e8 fb fc
    test al, al                               ; 84 c0
    jne short 06f13h                          ; 75 1a
    mov dx, ss                                ; 8c d2
    lea ax, [bp-006h]                         ; 8d 46 fa
    call 06c24h                               ; e8 23 fd
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    call 06bf0h                               ; e8 e8 fc
    mov dx, ss                                ; 8c d2
    lea ax, [bp-006h]                         ; 8d 46 fa
    call 06c24h                               ; e8 14 fd
    jmp near 070dah                           ; e9 c7 01
    or word [bp+018h], strict byte 00001h     ; 83 4e 18 01
    mov byte [bp+013h], 003h                  ; c6 46 13 03
    jmp near 070dah                           ; e9 bc 01
    or word [bp+018h], strict byte 00001h     ; 83 4e 18 01
    mov byte [bp+013h], 002h                  ; c6 46 13 02
    jmp near 070dah                           ; e9 b1 01
    cmp byte [bp+00dh], 004h                  ; 80 7e 0d 04
    jnc short 06f8ch                          ; 73 5d
    mov ax, 000e8h                            ; b8 e8 00
    call 06bf0h                               ; e8 bb fc
    test al, al                               ; 84 c0
    jne short 06f81h                          ; 75 48
    mov dx, ss                                ; 8c d2
    lea ax, [bp-008h]                         ; 8d 46 f8
    call 06c24h                               ; e8 e3 fc
    cmp byte [bp-008h], 0fah                  ; 80 7e f8 fa
    je short 06f57h                           ; 74 10
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    push ax                                   ; 50
    push 00936h                               ; 68 36 09
    push strict byte 00007h                   ; 6a 07
    call 01972h                               ; e8 1e aa
    add sp, strict byte 00006h                ; 83 c4 06
    movzx ax, byte [bp+00dh]                  ; 0f b6 46 0d
    call 06bf0h                               ; e8 92 fc
    mov dx, ss                                ; 8c d2
    lea ax, [bp-008h]                         ; 8d 46 f8
    call 06c24h                               ; e8 be fc
    cmp byte [bp-008h], 0fah                  ; 80 7e f8 fa
    je near 070dah                            ; 0f 84 6c 01
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    push ax                                   ; 50
    push 00936h                               ; 68 36 09
    push strict byte 00007h                   ; 6a 07
    call 01972h                               ; e8 f7 a9
    add sp, strict byte 00006h                ; 83 c4 06
    jmp near 070dah                           ; e9 59 01
    or word [bp+018h], strict byte 00001h     ; 83 4e 18 01
    mov byte [bp+013h], 003h                  ; c6 46 13 03
    jmp near 070dah                           ; e9 4e 01
    or word [bp+018h], strict byte 00001h     ; 83 4e 18 01
    mov byte [bp+013h], 002h                  ; c6 46 13 02
    jmp near 070dah                           ; e9 43 01
    mov ax, 000f2h                            ; b8 f2 00
    call 06bf0h                               ; e8 53 fc
    test al, al                               ; 84 c0
    jne short 06fb4h                          ; 75 13
    mov dx, ss                                ; 8c d2
    lea ax, [bp-008h]                         ; 8d 46 f8
    call 06c24h                               ; e8 7b fc
    mov dx, ss                                ; 8c d2
    lea ax, [bp-006h]                         ; 8d 46 fa
    call 06c24h                               ; e8 73 fc
    jmp near 06e8fh                           ; e9 db fe
    or word [bp+018h], strict byte 00001h     ; 83 4e 18 01
    mov byte [bp+013h], 003h                  ; c6 46 13 03
    jmp near 070dah                           ; e9 1b 01
    mov al, byte [bp+00dh]                    ; 8a 46 0d
    test al, al                               ; 84 c0
    jbe short 06fcdh                          ; 76 07
    cmp AL, strict byte 002h                  ; 3c 02
    jbe short 07036h                          ; 76 6c
    jmp near 0706ah                           ; e9 9d 00
    mov ax, 000e9h                            ; b8 e9 00
    call 06bf0h                               ; e8 1d fc
    test al, al                               ; 84 c0
    jne near 07060h                           ; 0f 85 87 00
    mov dx, ss                                ; 8c d2
    lea ax, [bp-008h]                         ; 8d 46 f8
    call 06c24h                               ; e8 43 fc
    mov cl, al                                ; 88 c1
    cmp byte [bp-008h], 0fah                  ; 80 7e f8 fa
    je short 06ff9h                           ; 74 10
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    push ax                                   ; 50
    push 00936h                               ; 68 36 09
    push strict byte 00007h                   ; 6a 07
    call 01972h                               ; e8 7c a9
    add sp, strict byte 00006h                ; 83 c4 06
    test cl, cl                               ; 84 c9
    jne short 07060h                          ; 75 63
    mov dx, ss                                ; 8c d2
    lea ax, [bp-008h]                         ; 8d 46 f8
    call 06c24h                               ; e8 1f fc
    test al, al                               ; 84 c0
    jne short 07060h                          ; 75 57
    mov dx, ss                                ; 8c d2
    lea ax, [bp-006h]                         ; 8d 46 fa
    call 06c24h                               ; e8 13 fc
    test al, al                               ; 84 c0
    jne short 07060h                          ; 75 4b
    mov dx, ss                                ; 8c d2
    lea ax, [bp-004h]                         ; 8d 46 fc
    call 06c24h                               ; e8 07 fc
    test al, al                               ; 84 c0
    jne short 07060h                          ; 75 3f
    mov al, byte [bp-008h]                    ; 8a 46 f8
    mov byte [bp+00ch], al                    ; 88 46 0c
    mov al, byte [bp-006h]                    ; 8a 46 fa
    mov byte [bp+010h], al                    ; 88 46 10
    mov al, byte [bp-004h]                    ; 8a 46 fc
    mov byte [bp+00eh], al                    ; 88 46 0e
    jmp near 070dah                           ; e9 a4 00
    cmp AL, strict byte 001h                  ; 3c 01
    jne short 0703fh                          ; 75 05
    mov ax, 000e6h                            ; b8 e6 00
    jmp short 07042h                          ; eb 03
    mov ax, 000e7h                            ; b8 e7 00
    call 06bf0h                               ; e8 ab fb
    mov cl, al                                ; 88 c1
    test cl, cl                               ; 84 c9
    jne short 0705ah                          ; 75 0f
    mov dx, ss                                ; 8c d2
    lea ax, [bp-008h]                         ; 8d 46 f8
    call 06c24h                               ; e8 d1 fb
    cmp byte [bp-008h], 0fah                  ; 80 7e f8 fa
    db  00fh, 095h, 0c1h
    ; setne cl                                  ; 0f 95 c1
    test cl, cl                               ; 84 c9
    je near 070dah                            ; 0f 84 7a 00
    or word [bp+018h], strict byte 00001h     ; 83 4e 18 01
    mov byte [bp+013h], 003h                  ; c6 46 13 03
    jmp short 070dah                          ; eb 70
    movzx ax, byte [bp+00dh]                  ; 0f b6 46 0d
    push ax                                   ; 50
    push 00962h                               ; 68 62 09
    push strict byte 00007h                   ; 6a 07
    call 01972h                               ; e8 fb a8
    add sp, strict byte 00006h                ; 83 c4 06
    or word [bp+018h], strict byte 00001h     ; 83 4e 18 01
    mov byte [bp+013h], 001h                  ; c6 46 13 01
    jmp short 070dah                          ; eb 56
    mov si, word [bp+00ch]                    ; 8b 76 0c
    mov bx, si                                ; 89 f3
    mov dx, strict word 00022h                ; ba 22 00
    mov ax, cx                                ; 89 c8
    call 0167ah                               ; e8 e9 a5
    mov bx, word [bp+014h]                    ; 8b 5e 14
    mov dx, strict word 00024h                ; ba 24 00
    mov ax, cx                                ; 89 c8
    call 0167ah                               ; e8 de a5
    mov dx, strict word 00027h                ; ba 27 00
    mov ax, cx                                ; 89 c8
    call 01650h                               ; e8 ac a5
    mov ah, al                                ; 88 c4
    test si, si                               ; 85 f6
    jne short 070b8h                          ; 75 0e
    cmp word [bp+014h], strict byte 00000h    ; 83 7e 14 00
    jne short 070b8h                          ; 75 08
    test AL, strict byte 080h                 ; a8 80
    je short 070bah                           ; 74 06
    and AL, strict byte 07fh                  ; 24 7f
    jmp short 070bah                          ; eb 02
    or AL, strict byte 080h                   ; 0c 80
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 00027h                ; ba 27 00
    mov ax, cx                                ; 89 c8
    call 0165eh                               ; e8 99 a5
    jmp short 070dah                          ; eb 13
    push 0097ch                               ; 68 7c 09
    push strict byte 00007h                   ; 6a 07
    call 01972h                               ; e8 a3 a8
    add sp, strict byte 00004h                ; 83 c4 04
    or word [bp+018h], strict byte 00001h     ; 83 4e 18 01
    mov byte [bp+013h], 001h                  ; c6 46 13 01
    mov ax, strict word 00047h                ; b8 47 00
    call 06c5fh                               ; e8 7f fb
    lea sp, [bp-002h]                         ; 8d 66 fe
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn                                      ; c3
_int17_function:                             ; 0xf70e6 LB 0xb3
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    push ax                                   ; 50
    sti                                       ; fb
    mov dx, word [bp+00eh]                    ; 8b 56 0e
    add dx, dx                                ; 01 d2
    add dx, strict byte 00008h                ; 83 c2 08
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 72 a5
    mov bx, ax                                ; 89 c3
    mov si, ax                                ; 89 c6
    cmp byte [bp+013h], 003h                  ; 80 7e 13 03
    jnc near 0718fh                           ; 0f 83 89 00
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    cmp ax, strict word 00003h                ; 3d 03 00
    jnc near 0718fh                           ; 0f 83 7f 00
    test bx, bx                               ; 85 db
    jbe near 0718fh                           ; 0f 86 79 00
    mov dx, ax                                ; 89 c2
    add dx, strict byte 00078h                ; 83 c2 78
    mov ax, strict word 00040h                ; b8 40 00
    call 01650h                               ; e8 2f a5
    movzx cx, al                              ; 0f b6 c8
    sal cx, 008h                              ; c1 e1 08
    cmp byte [bp+013h], 000h                  ; 80 7e 13 00
    jne short 0715ah                          ; 75 2d
    mov al, byte [bp+012h]                    ; 8a 46 12
    mov dx, bx                                ; 89 da
    out DX, AL                                ; ee
    lea dx, [bx+002h]                         ; 8d 57 02
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov word [bp-004h], ax                    ; 89 46 fc
    mov al, byte [bp-004h]                    ; 8a 46 fc
    or AL, strict byte 001h                   ; 0c 01
    out DX, AL                                ; ee
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc
    and AL, strict byte 0feh                  ; 24 fe
    out DX, AL                                ; ee
    lea dx, [si+001h]                         ; 8d 54 01
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 040h                 ; a8 40
    je short 0715ah                           ; 74 07
    test cx, cx                               ; 85 c9
    je short 0715ah                           ; 74 03
    dec cx                                    ; 49
    jmp short 07149h                          ; eb ef
    cmp byte [bp+013h], 001h                  ; 80 7e 13 01
    jne short 07176h                          ; 75 16
    lea dx, [si+002h]                         ; 8d 54 02
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov word [bp-004h], ax                    ; 89 46 fc
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc
    and AL, strict byte 0fbh                  ; 24 fb
    out DX, AL                                ; ee
    mov al, byte [bp-004h]                    ; 8a 46 fc
    or AL, strict byte 004h                   ; 0c 04
    out DX, AL                                ; ee
    lea dx, [si+001h]                         ; 8d 54 01
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    xor AL, strict byte 048h                  ; 34 48
    mov byte [bp+013h], al                    ; 88 46 13
    test cx, cx                               ; 85 c9
    jne short 07189h                          ; 75 04
    or byte [bp+013h], 001h                   ; 80 4e 13 01
    and byte [bp+01ch], 0feh                  ; 80 66 1c fe
    jmp short 07193h                          ; eb 04
    or byte [bp+01ch], 001h                   ; 80 4e 1c 01
    lea sp, [bp-002h]                         ; 8d 66 fe
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn                                      ; c3
wait_:                                       ; 0xf7199 LB 0xb2
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    sub sp, strict byte 0000ah                ; 83 ec 0a
    mov si, ax                                ; 89 c6
    mov byte [bp-00ch], dl                    ; 88 56 f4
    mov byte [bp-00ah], 000h                  ; c6 46 f6 00
    pushfw                                    ; 9c
    pop ax                                    ; 58
    mov word [bp-010h], ax                    ; 89 46 f0
    sti                                       ; fb
    xor cx, cx                                ; 31 c9
    mov dx, 0046ch                            ; ba 6c 04
    xor ax, ax                                ; 31 c0
    call 01688h                               ; e8 cc a4
    mov word [bp-00eh], ax                    ; 89 46 f2
    mov bx, dx                                ; 89 d3
    hlt                                       ; f4
    mov dx, 0046ch                            ; ba 6c 04
    xor ax, ax                                ; 31 c0
    call 01688h                               ; e8 be a4
    mov word [bp-012h], ax                    ; 89 46 ee
    mov di, dx                                ; 89 d7
    cmp dx, bx                                ; 39 da
    jnbe short 071dah                         ; 77 07
    jne short 071e1h                          ; 75 0c
    cmp ax, word [bp-00eh]                    ; 3b 46 f2
    jbe short 071e1h                          ; 76 07
    sub ax, word [bp-00eh]                    ; 2b 46 f2
    sbb dx, bx                                ; 19 da
    jmp short 071ech                          ; eb 0b
    cmp dx, bx                                ; 39 da
    jc short 071ech                           ; 72 07
    jne short 071f0h                          ; 75 09
    cmp ax, word [bp-00eh]                    ; 3b 46 f2
    jnc short 071f0h                          ; 73 04
    sub si, ax                                ; 29 c6
    sbb cx, dx                                ; 19 d1
    mov ax, word [bp-012h]                    ; 8b 46 ee
    mov word [bp-00eh], ax                    ; 89 46 f2
    mov bx, di                                ; 89 fb
    mov ax, 00100h                            ; b8 00 01
    int 016h                                  ; cd 16
    je near 07206h                            ; 0f 84 05 00
    mov AL, strict byte 001h                  ; b0 01
    jmp near 07208h                           ; e9 02 00
    db  032h, 0c0h
    ; xor al, al                                ; 32 c0
    test al, al                               ; 84 c0
    je short 07230h                           ; 74 24
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    int 016h                                  ; cd 16
    xchg ah, al                               ; 86 c4
    mov dl, al                                ; 88 c2
    mov byte [bp-00ah], al                    ; 88 46 f6
    movzx ax, dl                              ; 0f b6 c2
    push ax                                   ; 50
    push 0099eh                               ; 68 9e 09
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 4f a7
    add sp, strict byte 00006h                ; 83 c4 06
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00
    je short 07230h                           ; 74 04
    mov al, dl                                ; 88 d0
    jmp short 07242h                          ; eb 12
    test cx, cx                               ; 85 c9
    jnle short 071c1h                         ; 7f 8d
    jne short 0723ah                          ; 75 04
    test si, si                               ; 85 f6
    jnbe short 071c1h                         ; 77 87
    mov ax, word [bp-010h]                    ; 8b 46 f0
    push ax                                   ; 50
    popfw                                     ; 9d
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    lea sp, [bp-008h]                         ; 8d 66 f8
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
read_logo_byte_:                             ; 0xf724b LB 0x16
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push dx                                   ; 52
    xor ah, ah                                ; 30 e4
    or ah, 001h                               ; 80 cc 01
    mov dx, 003b8h                            ; ba b8 03
    out DX, ax                                ; ef
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    lea sp, [bp-002h]                         ; 8d 66 fe
    pop dx                                    ; 5a
    pop bp                                    ; 5d
    retn                                      ; c3
read_logo_word_:                             ; 0xf7261 LB 0x14
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push dx                                   ; 52
    xor ah, ah                                ; 30 e4
    or ah, 001h                               ; 80 cc 01
    mov dx, 003b8h                            ; ba b8 03
    out DX, ax                                ; ef
    in ax, DX                                 ; ed
    lea sp, [bp-002h]                         ; 8d 66 fe
    pop dx                                    ; 5a
    pop bp                                    ; 5d
    retn                                      ; c3
print_detected_harddisks_:                   ; 0xf7275 LB 0x130
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push si                                   ; 56
    push di                                   ; 57
    push ax                                   ; 50
    push ax                                   ; 50
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 e4 a3
    mov si, ax                                ; 89 c6
    mov byte [bp-00ch], 000h                  ; c6 46 f4 00
    xor cl, cl                                ; 30 c9
    xor ch, ch                                ; 30 ed
    mov dx, 002c0h                            ; ba c0 02
    call 01650h                               ; e8 b8 a3
    mov byte [bp-00eh], al                    ; 88 46 f2
    xor bl, bl                                ; 30 db
    cmp bl, byte [bp-00eh]                    ; 3a 5e f2
    jnc near 07377h                           ; 0f 83 d3 00
    movzx dx, bl                              ; 0f b6 d3
    add dx, 002c1h                            ; 81 c2 c1 02
    mov ax, si                                ; 89 f0
    call 01650h                               ; e8 a0 a3
    mov bh, al                                ; 88 c7
    cmp AL, strict byte 00ch                  ; 3c 0c
    jc short 072dah                           ; 72 24
    test cl, cl                               ; 84 c9
    jne short 072c7h                          ; 75 0d
    push 009afh                               ; 68 af 09
    push strict byte 00002h                   ; 6a 02
    call 01972h                               ; e8 b0 a6
    add sp, strict byte 00004h                ; 83 c4 04
    mov CL, strict byte 001h                  ; b1 01
    movzx ax, bl                              ; 0f b6 c3
    inc ax                                    ; 40
    push ax                                   ; 50
    push 009c3h                               ; 68 c3 09
    push strict byte 00002h                   ; 6a 02
    call 01972h                               ; e8 9e a6
    add sp, strict byte 00006h                ; 83 c4 06
    jmp near 07372h                           ; e9 98 00
    cmp AL, strict byte 008h                  ; 3c 08
    jc short 072f1h                           ; 72 13
    test ch, ch                               ; 84 ed
    jne short 072efh                          ; 75 0d
    push 009d6h                               ; 68 d6 09
    push strict byte 00002h                   ; 6a 02
    call 01972h                               ; e8 88 a6
    add sp, strict byte 00004h                ; 83 c4 04
    mov CH, strict byte 001h                  ; b5 01
    jmp short 072c7h                          ; eb d6
    cmp AL, strict byte 004h                  ; 3c 04
    jnc short 0730ch                          ; 73 17
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00
    jne short 0730ch                          ; 75 11
    push 009eah                               ; 68 ea 09
    push strict byte 00002h                   ; 6a 02
    call 01972h                               ; e8 6f a6
    add sp, strict byte 00004h                ; 83 c4 04
    mov byte [bp-00ch], 001h                  ; c6 46 f4 01
    jmp short 07322h                          ; eb 16
    cmp bh, 004h                              ; 80 ff 04
    jc short 07322h                           ; 72 11
    test cl, cl                               ; 84 c9
    jne short 07322h                          ; 75 0d
    push 009afh                               ; 68 af 09
    push strict byte 00002h                   ; 6a 02
    call 01972h                               ; e8 55 a6
    add sp, strict byte 00004h                ; 83 c4 04
    mov CL, strict byte 001h                  ; b1 01
    movzx ax, bl                              ; 0f b6 c3
    inc ax                                    ; 40
    push ax                                   ; 50
    push 009fbh                               ; 68 fb 09
    push strict byte 00002h                   ; 6a 02
    call 01972h                               ; e8 43 a6
    add sp, strict byte 00006h                ; 83 c4 06
    cmp bh, 004h                              ; 80 ff 04
    jc short 0733ah                           ; 72 03
    sub bh, 004h                              ; 80 ef 04
    movzx ax, bh                              ; 0f b6 c7
    cwd                                       ; 99
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2
    sar ax, 1                                 ; d1 f8
    test ax, ax                               ; 85 c0
    je short 0734bh                           ; 74 05
    push 00a05h                               ; 68 05 0a
    jmp short 0734eh                          ; eb 03
    push 00a10h                               ; 68 10 0a
    push strict byte 00002h                   ; 6a 02
    call 01972h                               ; e8 1f a6
    add sp, strict byte 00004h                ; 83 c4 04
    movzx ax, bh                              ; 0f b6 c7
    mov di, strict word 00002h                ; bf 02 00
    cwd                                       ; 99
    idiv di                                   ; f7 ff
    test dx, dx                               ; 85 d2
    je short 07368h                           ; 74 05
    push 00a19h                               ; 68 19 0a
    jmp short 0736bh                          ; eb 03
    push 00a1fh                               ; 68 1f 0a
    push di                                   ; 57
    call 01972h                               ; e8 03 a6
    add sp, strict byte 00004h                ; 83 c4 04
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3
    jmp near 0729dh                           ; e9 26 ff
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00
    jne short 07390h                          ; 75 13
    test cl, cl                               ; 84 c9
    jne short 07390h                          ; 75 0f
    test ch, ch                               ; 84 ed
    jne short 07390h                          ; 75 0b
    push 00a26h                               ; 68 26 0a
    push strict byte 00002h                   ; 6a 02
    call 01972h                               ; e8 e5 a5
    add sp, strict byte 00004h                ; 83 c4 04
    push 00a3ah                               ; 68 3a 0a
    push strict byte 00002h                   ; 6a 02
    call 01972h                               ; e8 da a5
    add sp, strict byte 00004h                ; 83 c4 04
    lea sp, [bp-00ah]                         ; 8d 66 f6
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
get_boot_drive_:                             ; 0xf73a5 LB 0x28
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    push dx                                   ; 52
    mov bl, al                                ; 88 c3
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 b7 a2
    mov dx, 002c0h                            ; ba c0 02
    call 01650h                               ; e8 95 a2
    sub bl, 002h                              ; 80 eb 02
    cmp bl, al                                ; 38 c3
    jc short 073c4h                           ; 72 02
    mov BL, strict byte 0ffh                  ; b3 ff
    mov al, bl                                ; 88 d8
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
show_logo_:                                  ; 0xf73cd LB 0x224
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push si                                   ; 56
    push di                                   ; 57
    sub sp, strict byte 0000ch                ; 83 ec 0c
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 8b a2
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
    call 07261h                               ; e8 68 fe
    cmp ax, 066bbh                            ; 3d bb 66
    jne near 074d1h                           ; 0f 85 d1 00
    push SS                                   ; 16
    pop ES                                    ; 07
    lea di, [bp-016h]                         ; 8d 7e ea
    mov ax, 04f03h                            ; b8 03 4f
    int 010h                                  ; cd 10
    mov word [es:di], bx                      ; 26 89 1d
    cmp ax, strict word 0004fh                ; 3d 4f 00
    jne near 074d1h                           ; 0f 85 bd 00
    mov al, dl                                ; 88 d0
    add AL, strict byte 004h                  ; 04 04
    xor ah, ah                                ; 30 e4
    call 0724bh                               ; e8 2e fe
    mov ch, al                                ; 88 c5
    mov byte [bp-00ch], al                    ; 88 46 f4
    mov al, dl                                ; 88 d0
    add AL, strict byte 005h                  ; 04 05
    xor ah, ah                                ; 30 e4
    call 0724bh                               ; e8 20 fe
    mov dh, al                                ; 88 c6
    mov byte [bp-010h], al                    ; 88 46 f0
    mov al, dl                                ; 88 d0
    add AL, strict byte 002h                  ; 04 02
    xor ah, ah                                ; 30 e4
    call 07261h                               ; e8 28 fe
    mov bx, ax                                ; 89 c3
    mov word [bp-014h], ax                    ; 89 46 ec
    mov al, dl                                ; 88 d0
    add AL, strict byte 006h                  ; 04 06
    xor ah, ah                                ; 30 e4
    call 0724bh                               ; e8 04 fe
    mov byte [bp-012h], al                    ; 88 46 ee
    test ch, ch                               ; 84 ed
    jne short 07458h                          ; 75 0a
    test dh, dh                               ; 84 f6
    jne short 07458h                          ; 75 06
    test bx, bx                               ; 85 db
    je near 074d1h                            ; 0f 84 79 00
    mov bx, 00142h                            ; bb 42 01
    mov ax, 04f02h                            ; b8 02 4f
    int 010h                                  ; cd 10
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00
    je short 07489h                           ; 74 23
    xor bx, bx                                ; 31 db
    jmp short 07470h                          ; eb 06
    inc bx                                    ; 43
    cmp bx, strict byte 00010h                ; 83 fb 10
    jnbe short 07490h                         ; 77 20
    mov ax, bx                                ; 89 d8
    or ah, 002h                               ; 80 cc 02
    mov dx, 003b8h                            ; ba b8 03
    out DX, ax                                ; ef
    xor dx, dx                                ; 31 d2
    mov ax, strict word 00001h                ; b8 01 00
    call 07199h                               ; e8 18 fd
    cmp AL, strict byte 086h                  ; 3c 86
    jne short 0746ah                          ; 75 e5
    mov CL, strict byte 001h                  ; b1 01
    jmp short 07490h                          ; eb 07
    mov ax, 00210h                            ; b8 10 02
    mov dx, 003b8h                            ; ba b8 03
    out DX, ax                                ; ef
    test cl, cl                               ; 84 c9
    jne short 074a6h                          ; 75 12
    mov ax, word [bp-014h]                    ; 8b 46 ec
    shr ax, 004h                              ; c1 e8 04
    mov dx, strict word 00001h                ; ba 01 00
    call 07199h                               ; e8 f9 fc
    cmp AL, strict byte 086h                  ; 3c 86
    jne short 074a6h                          ; 75 02
    mov CL, strict byte 001h                  ; b1 01
    cmp byte [bp-010h], 000h                  ; 80 7e f0 00
    je short 074d1h                           ; 74 25
    test cl, cl                               ; 84 c9
    jne short 074d1h                          ; 75 21
    mov bx, strict word 00010h                ; bb 10 00
    jmp short 074bah                          ; eb 05
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 074d1h                          ; 76 17
    mov ax, bx                                ; 89 d8
    or ah, 002h                               ; 80 cc 02
    mov dx, 003b8h                            ; ba b8 03
    out DX, ax                                ; ef
    xor dx, dx                                ; 31 d2
    mov ax, strict word 00001h                ; b8 01 00
    call 07199h                               ; e8 ce fc
    cmp AL, strict byte 086h                  ; 3c 86
    jne short 074b5h                          ; 75 e6
    mov CL, strict byte 001h                  ; b1 01
    xor bx, bx                                ; 31 db
    mov dx, 00339h                            ; ba 39 03
    mov ax, si                                ; 89 f0
    call 0165eh                               ; e8 83 a1
    mov AL, strict byte 003h                  ; b0 03
    mov AH, strict byte 000h                  ; b4 00
    int 010h                                  ; cd 10
    cmp byte [bp-012h], 000h                  ; 80 7e ee 00
    je near 075d2h                            ; 0f 84 e9 00
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00
    jne short 0751fh                          ; 75 30
    cmp byte [bp-010h], 000h                  ; 80 7e f0 00
    jne short 0751fh                          ; 75 2a
    cmp word [bp-014h], strict byte 00000h    ; 83 7e ec 00
    jne short 0751fh                          ; 75 24
    cmp byte [bp-012h], 002h                  ; 80 7e ee 02
    jne short 0750ch                          ; 75 0b
    push 00a3ch                               ; 68 3c 0a
    push strict byte 00002h                   ; 6a 02
    call 01972h                               ; e8 69 a4
    add sp, strict byte 00004h                ; 83 c4 04
    test cl, cl                               ; 84 c9
    jne short 0751fh                          ; 75 0f
    mov dx, strict word 00001h                ; ba 01 00
    mov ax, 000c0h                            ; b8 c0 00
    call 07199h                               ; e8 80 fc
    cmp AL, strict byte 086h                  ; 3c 86
    jne short 0751fh                          ; 75 02
    mov CL, strict byte 001h                  ; b1 01
    test cl, cl                               ; 84 c9
    je near 075d2h                            ; 0f 84 ad 00
    mov byte [bp-00eh], 000h                  ; c6 46 f2 00
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
    push 00a5eh                               ; 68 5e 0a
    push strict byte 00002h                   ; 6a 02
    call 01972h                               ; e8 24 a4
    add sp, strict byte 00004h                ; 83 c4 04
    call 07275h                               ; e8 21 fd
    push 00aa2h                               ; 68 a2 0a
    push strict byte 00002h                   ; 6a 02
    call 01972h                               ; e8 16 a4
    add sp, strict byte 00004h                ; 83 c4 04
    mov dx, strict word 00001h                ; ba 01 00
    mov ax, strict word 00040h                ; b8 40 00
    call 07199h                               ; e8 31 fc
    mov bl, al                                ; 88 c3
    test al, al                               ; 84 c0
    je short 0755fh                           ; 74 f1
    cmp AL, strict byte 030h                  ; 3c 30
    je short 075c0h                           ; 74 4e
    cmp bl, 002h                              ; 80 fb 02
    jc short 07599h                           ; 72 22
    cmp bl, 009h                              ; 80 fb 09
    jnbe short 07599h                         ; 77 1d
    movzx ax, bl                              ; 0f b6 c3
    call 073a5h                               ; e8 23 fe
    cmp AL, strict byte 0ffh                  ; 3c ff
    jne short 07588h                          ; 75 02
    jmp short 0755fh                          ; eb d7
    movzx bx, al                              ; 0f b6 d8
    mov dx, 00338h                            ; ba 38 03
    mov ax, si                                ; 89 f0
    call 0165eh                               ; e8 cb a0
    mov byte [bp-00eh], 002h                  ; c6 46 f2 02
    jmp short 075c0h                          ; eb 27
    cmp bl, 02eh                              ; 80 fb 2e
    je short 075aeh                           ; 74 10
    cmp bl, 026h                              ; 80 fb 26
    je short 075b4h                           ; 74 11
    cmp bl, 021h                              ; 80 fb 21
    jne short 075bah                          ; 75 12
    mov byte [bp-00eh], 001h                  ; c6 46 f2 01
    jmp short 075c0h                          ; eb 12
    mov byte [bp-00eh], 003h                  ; c6 46 f2 03
    jmp short 075c0h                          ; eb 0c
    mov byte [bp-00eh], 004h                  ; c6 46 f2 04
    jmp short 075c0h                          ; eb 06
    cmp byte [bp-00eh], 000h                  ; 80 7e f2 00
    je short 0755fh                           ; 74 9f
    movzx bx, byte [bp-00eh]                  ; 0f b6 5e f2
    mov dx, 00339h                            ; ba 39 03
    mov ax, si                                ; 89 f0
    call 0165eh                               ; e8 92 a0
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
    call 0edbfh                               ; e8 db 77
    pop DS                                    ; 1f
    popad                                     ; 66 61
    lea sp, [bp-00ah]                         ; 8d 66 f6
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
delay_boot_:                                 ; 0xf75f1 LB 0x67
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    push dx                                   ; 52
    mov dx, ax                                ; 89 c2
    test ax, ax                               ; 85 c0
    je short 07651h                           ; 74 55
    mov AL, strict byte 034h                  ; b0 34
    out strict byte 043h, AL                  ; e6 43
    mov AL, strict byte 0d3h                  ; b0 d3
    out strict byte 040h, AL                  ; e6 40
    mov AL, strict byte 048h                  ; b0 48
    out strict byte 040h, AL                  ; e6 40
    push dx                                   ; 52
    push 00aech                               ; 68 ec 0a
    push strict byte 00002h                   ; 6a 02
    call 01972h                               ; e8 61 a3
    add sp, strict byte 00006h                ; 83 c4 06
    mov bx, dx                                ; 89 d3
    test bx, bx                               ; 85 db
    jbe short 07631h                          ; 76 17
    push bx                                   ; 53
    push 00b0ah                               ; 68 0a 0b
    push strict byte 00002h                   ; 6a 02
    call 01972h                               ; e8 4f a3
    add sp, strict byte 00006h                ; 83 c4 06
    xor dx, dx                                ; 31 d2
    mov ax, strict word 00040h                ; b8 40 00
    call 07199h                               ; e8 6b fb
    dec bx                                    ; 4b
    jmp short 07616h                          ; eb e5
    push 00a3ah                               ; 68 3a 0a
    push strict byte 00002h                   ; 6a 02
    call 01972h                               ; e8 39 a3
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
    call 0edbfh                               ; e8 71 77
    pop DS                                    ; 1f
    popad                                     ; 66 61
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
scsi_cmd_data_in_:                           ; 0xf7658 LB 0xb2
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    push di                                   ; 57
    sub sp, strict byte 00006h                ; 83 ec 06
    mov si, ax                                ; 89 c6
    mov byte [bp-006h], dl                    ; 88 56 fa
    mov word [bp-00ah], bx                    ; 89 5e f6
    mov word [bp-008h], cx                    ; 89 4e f8
    mov bx, word [bp+00ah]                    ; 8b 5e 0a
    mov dx, si                                ; 89 f2
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 0766eh                          ; 75 f7
    mov ax, bx                                ; 89 d8
    mov dx, word [bp+00ch]                    ; 8b 56 0c
    mov cx, strict word 0000ch                ; b9 0c 00
    shr dx, 1                                 ; d1 ea
    rcr ax, 1                                 ; d1 d8
    loop 0767fh                               ; e2 fa
    and ax, 000f0h                            ; 25 f0 00
    movzx cx, byte [bp+004h]                  ; 0f b6 4e 04
    or cx, ax                                 ; 09 c1
    mov al, byte [bp-006h]                    ; 8a 46 fa
    mov dx, si                                ; 89 f2
    out DX, AL                                ; ee
    xor al, al                                ; 30 c0
    out DX, AL                                ; ee
    mov al, cl                                ; 88 c8
    out DX, AL                                ; ee
    mov al, bl                                ; 88 d8
    out DX, AL                                ; ee
    mov ax, bx                                ; 89 d8
    mov dx, word [bp+00ch]                    ; 8b 56 0c
    mov cx, strict word 00008h                ; b9 08 00
    shr dx, 1                                 ; d1 ea
    rcr ax, 1                                 ; d1 d8
    loop 076a5h                               ; e2 fa
    mov dx, si                                ; 89 f2
    out DX, AL                                ; ee
    xor cx, cx                                ; 31 c9
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04
    cmp cx, ax                                ; 39 c1
    jnc short 076c6h                          ; 73 0e
    les di, [bp-00ah]                         ; c4 7e f6
    add di, cx                                ; 01 cf
    mov al, byte [es:di]                      ; 26 8a 05
    mov dx, si                                ; 89 f2
    out DX, AL                                ; ee
    inc cx                                    ; 41
    jmp short 076b0h                          ; eb ea
    mov dx, si                                ; 89 f2
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 076c6h                          ; 75 f7
    lea dx, [si+001h]                         ; 8d 54 01
    cmp word [bp+00ch], strict byte 00000h    ; 83 7e 0c 00
    jne short 076deh                          ; 75 06
    cmp bx, 08000h                            ; 81 fb 00 80
    jbe short 076f8h                          ; 76 1a
    mov cx, 08000h                            ; b9 00 80
    les di, [bp+006h]                         ; c4 7e 06
    rep insb                                  ; f3 6c
    add bx, 08000h                            ; 81 c3 00 80
    adc word [bp+00ch], strict byte 0ffffh    ; 83 56 0c ff
    mov ax, es                                ; 8c c0
    add ax, 00800h                            ; 05 00 08
    mov word [bp+008h], ax                    ; 89 46 08
    jmp short 076cfh                          ; eb d7
    mov cx, bx                                ; 89 d9
    les di, [bp+006h]                         ; c4 7e 06
    rep insb                                  ; f3 6c
    xor ax, ax                                ; 31 c0
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn 0000ah                               ; c2 0a 00
scsi_cmd_data_out_:                          ; 0xf770a LB 0xb4
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    push di                                   ; 57
    sub sp, strict byte 00006h                ; 83 ec 06
    mov di, ax                                ; 89 c7
    mov byte [bp-006h], dl                    ; 88 56 fa
    mov word [bp-00ah], bx                    ; 89 5e f6
    mov word [bp-008h], cx                    ; 89 4e f8
    mov bx, word [bp+00ah]                    ; 8b 5e 0a
    mov dx, di                                ; 89 fa
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 07720h                          ; 75 f7
    mov ax, bx                                ; 89 d8
    mov dx, word [bp+00ch]                    ; 8b 56 0c
    mov cx, strict word 0000ch                ; b9 0c 00
    shr dx, 1                                 ; d1 ea
    rcr ax, 1                                 ; d1 d8
    loop 07731h                               ; e2 fa
    and ax, 000f0h                            ; 25 f0 00
    movzx cx, byte [bp+004h]                  ; 0f b6 4e 04
    or cx, ax                                 ; 09 c1
    mov al, byte [bp-006h]                    ; 8a 46 fa
    mov dx, di                                ; 89 fa
    out DX, AL                                ; ee
    mov AL, strict byte 001h                  ; b0 01
    out DX, AL                                ; ee
    mov al, cl                                ; 88 c8
    out DX, AL                                ; ee
    mov al, bl                                ; 88 d8
    out DX, AL                                ; ee
    mov ax, bx                                ; 89 d8
    mov dx, word [bp+00ch]                    ; 8b 56 0c
    mov cx, strict word 00008h                ; b9 08 00
    shr dx, 1                                 ; d1 ea
    rcr ax, 1                                 ; d1 d8
    loop 07757h                               ; e2 fa
    mov dx, di                                ; 89 fa
    out DX, AL                                ; ee
    xor cx, cx                                ; 31 c9
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04
    cmp cx, ax                                ; 39 c1
    jnc short 07778h                          ; 73 0e
    les si, [bp-00ah]                         ; c4 76 f6
    add si, cx                                ; 01 ce
    mov al, byte [es:si]                      ; 26 8a 04
    mov dx, di                                ; 89 fa
    out DX, AL                                ; ee
    inc cx                                    ; 41
    jmp short 07762h                          ; eb ea
    lea dx, [di+001h]                         ; 8d 55 01
    cmp word [bp+00ch], strict byte 00000h    ; 83 7e 0c 00
    jne short 07787h                          ; 75 06
    cmp bx, 08000h                            ; 81 fb 00 80
    jbe short 077a2h                          ; 76 1b
    mov cx, 08000h                            ; b9 00 80
    les si, [bp+006h]                         ; c4 76 06
    db  0f3h, 026h, 06eh
    ; rep es outsb                              ; f3 26 6e
    add bx, 08000h                            ; 81 c3 00 80
    adc word [bp+00ch], strict byte 0ffffh    ; 83 56 0c ff
    mov ax, es                                ; 8c c0
    add ax, 00800h                            ; 05 00 08
    mov word [bp+008h], ax                    ; 89 46 08
    jmp short 07778h                          ; eb d6
    mov cx, bx                                ; 89 d9
    les si, [bp+006h]                         ; c4 76 06
    db  0f3h, 026h, 06eh
    ; rep es outsb                              ; f3 26 6e
    mov dx, di                                ; 89 fa
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 077aah                          ; 75 f7
    xor ax, ax                                ; 31 c0
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn 0000ah                               ; c2 0a 00
@scsi_read_sectors:                          ; 0xf77be LB 0xb9
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    push di                                   ; 57
    sub sp, strict byte 0000eh                ; 83 ec 0e
    mov si, word [bp+004h]                    ; 8b 76 04
    mov es, [bp+006h]                         ; 8e 46 06
    mov bl, byte [es:si+008h]                 ; 26 8a 5c 08
    sub bl, 008h                              ; 80 eb 08
    cmp bl, 004h                              ; 80 fb 04
    jbe short 077eah                          ; 76 12
    movzx ax, bl                              ; 0f b6 c3
    push ax                                   ; 50
    push 00b0eh                               ; 68 0e 0b
    push 00b20h                               ; 68 20 0b
    push strict byte 00007h                   ; 6a 07
    call 01972h                               ; e8 8b a1
    add sp, strict byte 00008h                ; 83 c4 08
    mov es, [bp+006h]                         ; 8e 46 06
    mov di, word [es:si+00ah]                 ; 26 8b 7c 0a
    mov word [bp-012h], strict word 00028h    ; c7 46 ee 28 00
    mov ax, word [es:si]                      ; 26 8b 04
    mov dx, word [es:si+002h]                 ; 26 8b 54 02
    xchg ah, al                               ; 86 c4
    xchg dh, dl                               ; 86 d6
    xchg dx, ax                               ; 92
    mov word [bp-010h], ax                    ; 89 46 f0
    mov word [bp-00eh], dx                    ; 89 56 f2
    mov byte [bp-00ch], 000h                  ; c6 46 f4 00
    mov ax, di                                ; 89 f8
    xchg ah, al                               ; 86 c4
    mov word [bp-00bh], ax                    ; 89 46 f5
    mov byte [bp-009h], 000h                  ; c6 46 f7 00
    xor bh, bh                                ; 30 ff
    sal bx, 002h                              ; c1 e3 02
    add bx, si                                ; 01 f3
    mov ax, word [es:bx+001d8h]               ; 26 8b 87 d8 01
    mov dl, byte [es:bx+001dah]               ; 26 8a 97 da 01
    mov word [bp-008h], di                    ; 89 7e f8
    mov word [bp-006h], strict word 00000h    ; c7 46 fa 00 00
    mov cx, strict word 00009h                ; b9 09 00
    sal word [bp-008h], 1                     ; d1 66 f8
    rcl word [bp-006h], 1                     ; d1 56 fa
    loop 07833h                               ; e2 f8
    push dword [bp-008h]                      ; 66 ff 76 f8
    db  066h, 026h, 0ffh, 074h, 004h
    ; push dword [es:si+004h]                   ; 66 26 ff 74 04
    push strict byte 0000ah                   ; 6a 0a
    xor dh, dh                                ; 30 f6
    mov cx, ss                                ; 8c d1
    lea bx, [bp-012h]                         ; 8d 5e ee
    call 07658h                               ; e8 08 fe
    mov ah, al                                ; 88 c4
    test al, al                               ; 84 c0
    jne short 0786bh                          ; 75 15
    mov es, [bp+006h]                         ; 8e 46 06
    mov word [es:si+014h], di                 ; 26 89 7c 14
    mov dx, word [bp-008h]                    ; 8b 56 f8
    mov word [es:si+016h], dx                 ; 26 89 54 16
    mov dx, word [bp-006h]                    ; 8b 56 fa
    mov word [es:si+018h], dx                 ; 26 89 54 18
    movzx ax, ah                              ; 0f b6 c4
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn 00004h                               ; c2 04 00
@scsi_write_sectors:                         ; 0xf7877 LB 0xb9
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    push di                                   ; 57
    sub sp, strict byte 0000eh                ; 83 ec 0e
    mov si, word [bp+004h]                    ; 8b 76 04
    mov es, [bp+006h]                         ; 8e 46 06
    mov bl, byte [es:si+008h]                 ; 26 8a 5c 08
    sub bl, 008h                              ; 80 eb 08
    cmp bl, 004h                              ; 80 fb 04
    jbe short 078a3h                          ; 76 12
    movzx ax, bl                              ; 0f b6 c3
    push ax                                   ; 50
    push 00b3fh                               ; 68 3f 0b
    push 00b20h                               ; 68 20 0b
    push strict byte 00007h                   ; 6a 07
    call 01972h                               ; e8 d2 a0
    add sp, strict byte 00008h                ; 83 c4 08
    mov es, [bp+006h]                         ; 8e 46 06
    mov di, word [es:si+00ah]                 ; 26 8b 7c 0a
    mov word [bp-012h], strict word 0002ah    ; c7 46 ee 2a 00
    mov ax, word [es:si]                      ; 26 8b 04
    mov dx, word [es:si+002h]                 ; 26 8b 54 02
    xchg ah, al                               ; 86 c4
    xchg dh, dl                               ; 86 d6
    xchg dx, ax                               ; 92
    mov word [bp-010h], ax                    ; 89 46 f0
    mov word [bp-00eh], dx                    ; 89 56 f2
    mov byte [bp-00ch], 000h                  ; c6 46 f4 00
    mov ax, di                                ; 89 f8
    xchg ah, al                               ; 86 c4
    mov word [bp-00bh], ax                    ; 89 46 f5
    mov byte [bp-009h], 000h                  ; c6 46 f7 00
    xor bh, bh                                ; 30 ff
    sal bx, 002h                              ; c1 e3 02
    add bx, si                                ; 01 f3
    mov ax, word [es:bx+001d8h]               ; 26 8b 87 d8 01
    mov dl, byte [es:bx+001dah]               ; 26 8a 97 da 01
    mov word [bp-008h], di                    ; 89 7e f8
    mov word [bp-006h], strict word 00000h    ; c7 46 fa 00 00
    mov cx, strict word 00009h                ; b9 09 00
    sal word [bp-008h], 1                     ; d1 66 f8
    rcl word [bp-006h], 1                     ; d1 56 fa
    loop 078ech                               ; e2 f8
    push dword [bp-008h]                      ; 66 ff 76 f8
    db  066h, 026h, 0ffh, 074h, 004h
    ; push dword [es:si+004h]                   ; 66 26 ff 74 04
    push strict byte 0000ah                   ; 6a 0a
    xor dh, dh                                ; 30 f6
    mov cx, ss                                ; 8c d1
    lea bx, [bp-012h]                         ; 8d 5e ee
    call 0770ah                               ; e8 01 fe
    mov ah, al                                ; 88 c4
    test al, al                               ; 84 c0
    jne short 07924h                          ; 75 15
    mov es, [bp+006h]                         ; 8e 46 06
    mov word [es:si+014h], di                 ; 26 89 7c 14
    mov dx, word [bp-008h]                    ; 8b 56 f8
    mov word [es:si+016h], dx                 ; 26 89 54 16
    mov dx, word [bp-006h]                    ; 8b 56 fa
    mov word [es:si+018h], dx                 ; 26 89 54 18
    movzx ax, ah                              ; 0f b6 c4
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn 00004h                               ; c2 04 00
scsi_cmd_packet_:                            ; 0xf7930 LB 0x166
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    push di                                   ; 57
    sub sp, strict byte 0000ch                ; 83 ec 0c
    mov di, ax                                ; 89 c7
    mov byte [bp-006h], dl                    ; 88 56 fa
    mov word [bp-00ch], bx                    ; 89 5e f4
    mov word [bp-00ah], cx                    ; 89 4e f6
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 20 9d
    mov si, 00122h                            ; be 22 01
    mov word [bp-00eh], ax                    ; 89 46 f2
    cmp byte [bp+00ah], 002h                  ; 80 7e 0a 02
    jne short 07977h                          ; 75 1f
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 ce 9f
    push 00b52h                               ; 68 52 0b
    push 00b62h                               ; 68 62 0b
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 04 a0
    add sp, strict byte 00006h                ; 83 c4 06
    mov dx, strict word 00001h                ; ba 01 00
    jmp near 07a8bh                           ; e9 14 01
    sub di, strict byte 00008h                ; 83 ef 08
    sal di, 002h                              ; c1 e7 02
    sub byte [bp-006h], 002h                  ; 80 6e fa 02
    mov es, [bp-00eh]                         ; 8e 46 f2
    add di, si                                ; 01 f7
    mov bx, word [es:di+001d8h]               ; 26 8b 9d d8 01
    mov al, byte [es:di+001dah]               ; 26 8a 85 da 01
    mov byte [bp-008h], al                    ; 88 46 f8
    mov dx, bx                                ; 89 da
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 07993h                          ; 75 f7
    xor ax, ax                                ; 31 c0
    mov dx, word [bp+006h]                    ; 8b 56 06
    add dx, word [bp+004h]                    ; 03 56 04
    adc ax, word [bp+008h]                    ; 13 46 08
    mov es, [bp-00eh]                         ; 8e 46 f2
    mov cx, word [es:si+01ch]                 ; 26 8b 4c 1c
    xor di, di                                ; 31 ff
    add cx, dx                                ; 01 d1
    mov word [bp-010h], cx                    ; 89 4e f0
    adc di, ax                                ; 11 c7
    mov ax, cx                                ; 89 c8
    mov dx, di                                ; 89 fa
    mov cx, strict word 0000ch                ; b9 0c 00
    shr dx, 1                                 ; d1 ea
    rcr ax, 1                                 ; d1 d8
    loop 079beh                               ; e2 fa
    and ax, 000f0h                            ; 25 f0 00
    movzx cx, byte [bp-006h]                  ; 0f b6 4e fa
    or cx, ax                                 ; 09 c1
    mov al, byte [bp-008h]                    ; 8a 46 f8
    mov dx, bx                                ; 89 da
    out DX, AL                                ; ee
    xor al, al                                ; 30 c0
    out DX, AL                                ; ee
    mov al, cl                                ; 88 c8
    out DX, AL                                ; ee
    mov al, byte [bp-010h]                    ; 8a 46 f0
    out DX, AL                                ; ee
    mov ax, word [bp-010h]                    ; 8b 46 f0
    mov dx, di                                ; 89 fa
    mov cx, strict word 00008h                ; b9 08 00
    shr dx, 1                                 ; d1 ea
    rcr ax, 1                                 ; d1 d8
    loop 079e5h                               ; e2 fa
    mov dx, bx                                ; 89 da
    out DX, AL                                ; ee
    xor cx, cx                                ; 31 c9
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa
    cmp cx, ax                                ; 39 c1
    jnc short 07a06h                          ; 73 0e
    les di, [bp-00ch]                         ; c4 7e f4
    add di, cx                                ; 01 cf
    mov al, byte [es:di]                      ; 26 8a 05
    mov dx, bx                                ; 89 da
    out DX, AL                                ; ee
    inc cx                                    ; 41
    jmp short 079f0h                          ; eb ea
    mov dx, bx                                ; 89 da
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 07a06h                          ; 75 f7
    test AL, strict byte 002h                 ; a8 02
    je short 07a21h                           ; 74 0e
    lea dx, [bx+003h]                         ; 8d 57 03
    xor al, al                                ; 30 c0
    out DX, AL                                ; ee
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov dx, strict word 00003h                ; ba 03 00
    jmp short 07a8bh                          ; eb 6a
    mov ax, word [bp+004h]                    ; 8b 46 04
    test ax, ax                               ; 85 c0
    je short 07a30h                           ; 74 08
    lea dx, [bx+001h]                         ; 8d 57 01
    mov cx, ax                                ; 89 c1
    in AL, DX                                 ; ec
    loop 07a2dh                               ; e2 fd
    mov ax, word [bp+006h]                    ; 8b 46 06
    mov es, [bp-00eh]                         ; 8e 46 f2
    mov word [es:si+016h], ax                 ; 26 89 44 16
    mov ax, word [bp+008h]                    ; 8b 46 08
    mov word [es:si+018h], ax                 ; 26 89 44 18
    lea ax, [bx+001h]                         ; 8d 47 01
    cmp word [bp+008h], strict byte 00000h    ; 83 7e 08 00
    jne short 07a51h                          ; 75 07
    cmp word [bp+006h], 08000h                ; 81 7e 06 00 80
    jbe short 07a6eh                          ; 76 1d
    mov dx, ax                                ; 89 c2
    mov cx, 08000h                            ; b9 00 80
    les di, [bp+00ch]                         ; c4 7e 0c
    rep insb                                  ; f3 6c
    add word [bp+006h], 08000h                ; 81 46 06 00 80
    adc word [bp+008h], strict byte 0ffffh    ; 83 56 08 ff
    mov ax, es                                ; 8c c0
    add ax, 00800h                            ; 05 00 08
    mov word [bp+00eh], ax                    ; 89 46 0e
    jmp short 07a41h                          ; eb d3
    mov dx, ax                                ; 89 c2
    mov cx, word [bp+006h]                    ; 8b 4e 06
    les di, [bp+00ch]                         ; c4 7e 0c
    rep insb                                  ; f3 6c
    mov es, [bp-00eh]                         ; 8e 46 f2
    cmp word [es:si+01ch], strict byte 00000h ; 26 83 7c 1c 00
    je short 07a89h                           ; 74 07
    mov cx, word [es:si+01ch]                 ; 26 8b 4c 1c
    in AL, DX                                 ; ec
    loop 07a86h                               ; e2 fd
    xor dx, dx                                ; 31 d2
    mov ax, dx                                ; 89 d0
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn 0000ch                               ; c2 0c 00
scsi_enumerate_attached_devices_:            ; 0xf7a96 LB 0x3e5
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push si                                   ; 56
    push di                                   ; 57
    sub sp, 0021ch                            ; 81 ec 1c 02
    push ax                                   ; 50
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 c0 9b
    mov si, 00122h                            ; be 22 01
    mov word [bp-01ah], ax                    ; 89 46 e6
    mov word [bp-010h], strict word 00000h    ; c7 46 f0 00 00
    jmp near 07e09h                           ; e9 4f 03
    mov es, [bp-01ah]                         ; 8e 46 e6
    cmp byte [es:si+001e8h], 004h             ; 26 80 bc e8 01 04
    jnc near 07e71h                           ; 0f 83 aa 03
    mov cx, strict word 0000ah                ; b9 0a 00
    xor bx, bx                                ; 31 db
    mov dx, ss                                ; 8c d2
    lea ax, [bp-026h]                         ; 8d 46 da
    call 094dah                               ; e8 06 1a
    mov byte [bp-026h], 025h                  ; c6 46 da 25
    push dword 000000008h                     ; 66 6a 08
    lea dx, [bp-00226h]                       ; 8d 96 da fd
    push SS                                   ; 16
    push dx                                   ; 52
    push strict byte 0000ah                   ; 6a 0a
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0
    mov cx, ss                                ; 8c d1
    lea bx, [bp-026h]                         ; 8d 5e da
    mov ax, word [bp-00228h]                  ; 8b 86 d8 fd
    call 07658h                               ; e8 65 fb
    test al, al                               ; 84 c0
    je short 07b05h                           ; 74 0e
    push 00b82h                               ; 68 82 0b
    push 00bbbh                               ; 68 bb 0b
    push strict byte 00007h                   ; 6a 07
    call 01972h                               ; e8 70 9e
    add sp, strict byte 00006h                ; 83 c4 06
    movzx ax, byte [bp-00225h]                ; 0f b6 86 db fd
    movzx di, byte [bp-00226h]                ; 0f b6 be da fd
    sal di, 008h                              ; c1 e7 08
    xor bx, bx                                ; 31 db
    or di, ax                                 ; 09 c7
    movzx ax, byte [bp-00224h]                ; 0f b6 86 dc fd
    xor dx, dx                                ; 31 d2
    mov cx, strict word 00008h                ; b9 08 00
    sal ax, 1                                 ; d1 e0
    rcl dx, 1                                 ; d1 d2
    loop 07b20h                               ; e2 fa
    or bx, ax                                 ; 09 c3
    or dx, di                                 ; 09 fa
    movzx ax, byte [bp-00223h]                ; 0f b6 86 dd fd
    or bx, ax                                 ; 09 c3
    mov word [bp-00eh], bx                    ; 89 5e f2
    mov word [bp-018h], dx                    ; 89 56 e8
    movzx di, byte [bp-00222h]                ; 0f b6 be de fd
    sal di, 008h                              ; c1 e7 08
    movzx dx, byte [bp-00221h]                ; 0f b6 96 df fd
    xor bx, bx                                ; 31 db
    or di, dx                                 ; 09 d7
    movzx ax, byte [bp-00220h]                ; 0f b6 86 e0 fd
    xor dx, dx                                ; 31 d2
    mov cx, strict word 00008h                ; b9 08 00
    sal ax, 1                                 ; d1 e0
    rcl dx, 1                                 ; d1 d2
    loop 07b52h                               ; e2 fa
    or ax, bx                                 ; 09 d8
    or dx, di                                 ; 09 fa
    movzx bx, byte [bp-0021fh]                ; 0f b6 9e e1 fd
    or ax, bx                                 ; 09 d8
    mov word [bp-016h], ax                    ; 89 46 ea
    test dx, dx                               ; 85 d2
    jne short 07b6fh                          ; 75 05
    cmp ax, 00200h                            ; 3d 00 02
    je short 07b8fh                           ; 74 20
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 b7 9d
    push dx                                   ; 52
    push word [bp-016h]                       ; ff 76 ea
    push word [bp-010h]                       ; ff 76 f0
    push 00bdah                               ; 68 da 0b
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 e9 9d
    add sp, strict byte 0000ah                ; 83 c4 0a
    jmp near 07e00h                           ; e9 71 02
    mov es, [bp-01ah]                         ; 8e 46 e6
    mov al, byte [es:si+001e8h]               ; 26 8a 84 e8 01
    mov byte [bp-00ch], al                    ; 88 46 f4
    cmp AL, strict byte 001h                  ; 3c 01
    jc short 07baah                           ; 72 0c
    jbe short 07bb2h                          ; 76 12
    cmp AL, strict byte 003h                  ; 3c 03
    je short 07bbah                           ; 74 16
    cmp AL, strict byte 002h                  ; 3c 02
    je short 07bb6h                           ; 74 0e
    jmp short 07c06h                          ; eb 5c
    test al, al                               ; 84 c0
    jne short 07c06h                          ; 75 58
    mov BL, strict byte 090h                  ; b3 90
    jmp short 07bbch                          ; eb 0a
    mov BL, strict byte 098h                  ; b3 98
    jmp short 07bbch                          ; eb 06
    mov BL, strict byte 0a0h                  ; b3 a0
    jmp short 07bbch                          ; eb 02
    mov BL, strict byte 0a8h                  ; b3 a8
    mov al, bl                                ; 88 d8
    add AL, strict byte 007h                  ; 04 07
    movzx cx, al                              ; 0f b6 c8
    mov ax, cx                                ; 89 c8
    call 016ach                               ; e8 e4 9a
    test al, al                               ; 84 c0
    je short 07c06h                           ; 74 3a
    mov al, bl                                ; 88 d8
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    xor ah, ah                                ; 30 e4
    call 016ach                               ; e8 d7 9a
    xor ah, ah                                ; 30 e4
    mov dx, ax                                ; 89 c2
    sal dx, 008h                              ; c1 e2 08
    movzx ax, bl                              ; 0f b6 c3
    call 016ach                               ; e8 ca 9a
    xor ah, ah                                ; 30 e4
    add ax, dx                                ; 01 d0
    cwd                                       ; 99
    mov di, ax                                ; 89 c7
    mov word [bp-012h], dx                    ; 89 56 ee
    mov al, bl                                ; 88 d8
    add AL, strict byte 002h                  ; 04 02
    xor ah, ah                                ; 30 e4
    call 016ach                               ; e8 b7 9a
    xor ah, ah                                ; 30 e4
    mov word [bp-014h], ax                    ; 89 46 ec
    mov ax, cx                                ; 89 c8
    call 016ach                               ; e8 ad 9a
    xor ah, ah                                ; 30 e4
    mov word [bp-01ch], ax                    ; 89 46 e4
    jmp short 07c4bh                          ; eb 45
    mov ax, word [bp-018h]                    ; 8b 46 e8
    cmp ax, strict word 00040h                ; 3d 40 00
    jnbe short 07c10h                         ; 77 02
    jne short 07c1ch                          ; 75 0c
    mov word [bp-014h], 000ffh                ; c7 46 ec ff 00
    mov word [bp-01ch], strict word 0003fh    ; c7 46 e4 3f 00
    jmp short 07c34h                          ; eb 18
    cmp ax, strict word 00020h                ; 3d 20 00
    jnbe short 07c23h                         ; 77 02
    jne short 07c2ah                          ; 75 07
    mov word [bp-014h], 00080h                ; c7 46 ec 80 00
    jmp short 07c2fh                          ; eb 05
    mov word [bp-014h], strict word 00040h    ; c7 46 ec 40 00
    mov word [bp-01ch], strict word 00020h    ; c7 46 e4 20 00
    mov bx, word [bp-014h]                    ; 8b 5e ec
    imul bx, word [bp-01ch]                   ; 0f af 5e e4
    mov ax, word [bp-00eh]                    ; 8b 46 f2
    mov dx, word [bp-018h]                    ; 8b 56 e8
    xor cx, cx                                ; 31 c9
    call 09470h                               ; e8 2a 18
    mov di, ax                                ; 89 c7
    mov word [bp-012h], dx                    ; 89 56 ee
    mov dl, byte [bp-00ch]                    ; 8a 56 f4
    add dl, 008h                              ; 80 c2 08
    movzx bx, byte [bp-00ch]                  ; 0f b6 5e f4
    sal bx, 002h                              ; c1 e3 02
    mov es, [bp-01ah]                         ; 8e 46 e6
    add bx, si                                ; 01 f3
    mov ax, word [bp-00228h]                  ; 8b 86 d8 fd
    mov word [es:bx+001d8h], ax               ; 26 89 87 d8 01
    mov al, byte [bp-010h]                    ; 8a 46 f0
    mov byte [es:bx+001dah], al               ; 26 88 87 da 01
    movzx ax, dl                              ; 0f b6 c2
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    db  066h, 026h, 0c7h, 047h, 01eh, 004h, 0ffh, 000h, 000h
    ; mov dword [es:bx+01eh], strict dword 00000ff04h ; 66 26 c7 47 1e 04 ff 00 00
    mov ax, word [bp-016h]                    ; 8b 46 ea
    mov word [es:bx+024h], ax                 ; 26 89 47 24
    mov byte [es:bx+023h], 001h               ; 26 c6 47 23 01
    mov ax, word [bp-014h]                    ; 8b 46 ec
    mov word [es:bx+026h], ax                 ; 26 89 47 26
    mov ax, word [bp-01ch]                    ; 8b 46 e4
    mov word [es:bx+02ah], ax                 ; 26 89 47 2a
    cmp word [bp-012h], strict byte 00000h    ; 83 7e ee 00
    jne short 07ca7h                          ; 75 06
    cmp di, 00400h                            ; 81 ff 00 04
    jbe short 07cafh                          ; 76 08
    mov word [es:bx+028h], 00400h             ; 26 c7 47 28 00 04
    jmp short 07cb3h                          ; eb 04
    mov word [es:bx+028h], di                 ; 26 89 7f 28
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 73 9c
    push word [bp-018h]                       ; ff 76 e8
    push word [bp-00eh]                       ; ff 76 f2
    push word [bp-01ch]                       ; ff 76 e4
    push word [bp-014h]                       ; ff 76 ec
    push di                                   ; 57
    push word [bp-010h]                       ; ff 76 f0
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4
    push ax                                   ; 50
    push 00c08h                               ; 68 08 0c
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 97 9c
    add sp, strict byte 00012h                ; 83 c4 12
    movzx ax, dl                              ; 0f b6 c2
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    mov es, [bp-01ah]                         ; 8e 46 e6
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    mov ax, word [bp-014h]                    ; 8b 46 ec
    mov word [es:bx+02ch], ax                 ; 26 89 47 2c
    mov ax, word [bp-01ch]                    ; 8b 46 e4
    mov word [es:bx+030h], ax                 ; 26 89 47 30
    cmp word [bp-012h], strict byte 00000h    ; 83 7e ee 00
    jne short 07d05h                          ; 75 06
    cmp di, 00400h                            ; 81 ff 00 04
    jbe short 07d0dh                          ; 76 08
    mov word [es:bx+02eh], 00400h             ; 26 c7 47 2e 00 04
    jmp short 07d11h                          ; eb 04
    mov word [es:bx+02eh], di                 ; 26 89 7f 2e
    movzx ax, dl                              ; 0f b6 c2
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    mov es, [bp-01ah]                         ; 8e 46 e6
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    mov ax, word [bp-00eh]                    ; 8b 46 f2
    mov word [es:bx+032h], ax                 ; 26 89 47 32
    mov ax, word [bp-018h]                    ; 8b 46 e8
    mov word [es:bx+034h], ax                 ; 26 89 47 34
    mov al, byte [es:si+0019eh]               ; 26 8a 84 9e 01
    mov ah, byte [bp-00ch]                    ; 8a 66 f4
    add ah, 008h                              ; 80 c4 08
    movzx bx, al                              ; 0f b6 d8
    add bx, si                                ; 01 f3
    mov byte [es:bx+0019fh], ah               ; 26 88 a7 9f 01
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    mov byte [es:si+0019eh], al               ; 26 88 84 9e 01
    mov dx, strict word 00075h                ; ba 75 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01650h                               ; e8 ff 98
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 00075h                ; ba 75 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 ff 98
    inc byte [bp-00ch]                        ; fe 46 f4
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    mov es, [bp-01ah]                         ; 8e 46 e6
    mov byte [es:si+001e8h], al               ; 26 88 84 e8 01
    jmp near 07e00h                           ; e9 90 00
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 b6 9b
    push word [bp-010h]                       ; ff 76 f0
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4
    push ax                                   ; 50
    push 00c32h                               ; 68 32 0c
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 e7 9b
    add sp, strict byte 00008h                ; 83 c4 08
    mov dl, byte [bp-00ch]                    ; 8a 56 f4
    add dl, 008h                              ; 80 c2 08
    test byte [bp-00225h], 080h               ; f6 86 db fd 80
    db  00fh, 095h, 0c0h
    ; setne al                                  ; 0f 95 c0
    xor ah, ah                                ; 30 e4
    mov cx, ax                                ; 89 c1
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4
    sal ax, 002h                              ; c1 e0 02
    mov es, [bp-01ah]                         ; 8e 46 e6
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    mov ax, word [bp-00228h]                  ; 8b 86 d8 fd
    mov word [es:bx+001d8h], ax               ; 26 89 87 d8 01
    mov al, byte [bp-010h]                    ; 8a 46 f0
    mov byte [es:bx+001dah], al               ; 26 88 87 da 01
    movzx ax, dl                              ; 0f b6 c2
    imul ax, ax, strict byte 00018h           ; 6b c0 18
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    mov word [es:bx+01eh], 00504h             ; 26 c7 47 1e 04 05
    mov byte [es:bx+020h], cl                 ; 26 88 4f 20
    mov word [es:bx+024h], 00800h             ; 26 c7 47 24 00 08
    mov al, byte [es:si+001afh]               ; 26 8a 84 af 01
    mov ah, byte [bp-00ch]                    ; 8a 66 f4
    add ah, 008h                              ; 80 c4 08
    movzx bx, al                              ; 0f b6 d8
    add bx, si                                ; 01 f3
    mov byte [es:bx+001b0h], ah               ; 26 88 a7 b0 01
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    mov byte [es:si+001afh], al               ; 26 88 84 af 01
    inc byte [bp-00ch]                        ; fe 46 f4
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    mov byte [es:si+001e8h], al               ; 26 88 84 e8 01
    inc word [bp-010h]                        ; ff 46 f0
    cmp word [bp-010h], strict byte 00010h    ; 83 7e f0 10
    jnl short 07e71h                          ; 7d 68
    mov byte [bp-026h], 012h                  ; c6 46 da 12
    xor al, al                                ; 30 c0
    mov byte [bp-025h], al                    ; 88 46 db
    mov byte [bp-024h], al                    ; 88 46 dc
    mov byte [bp-023h], al                    ; 88 46 dd
    mov byte [bp-022h], 005h                  ; c6 46 de 05
    mov byte [bp-021h], al                    ; 88 46 df
    push dword 000000005h                     ; 66 6a 05
    lea dx, [bp-00226h]                       ; 8d 96 da fd
    push SS                                   ; 16
    push dx                                   ; 52
    push strict byte 00006h                   ; 6a 06
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0
    mov cx, ss                                ; 8c d1
    lea bx, [bp-026h]                         ; 8d 5e da
    mov ax, word [bp-00228h]                  ; 8b 86 d8 fd
    call 07658h                               ; e8 1e f8
    test al, al                               ; 84 c0
    je short 07e4ch                           ; 74 0e
    push 00b82h                               ; 68 82 0b
    push 00ba2h                               ; 68 a2 0b
    push strict byte 00007h                   ; 6a 07
    call 01972h                               ; e8 29 9b
    add sp, strict byte 00006h                ; 83 c4 06
    test byte [bp-00226h], 0e0h               ; f6 86 da fd e0
    jne short 07e5ch                          ; 75 09
    test byte [bp-00226h], 01fh               ; f6 86 da fd 1f
    je near 07abah                            ; 0f 84 5e fc
    test byte [bp-00226h], 0e0h               ; f6 86 da fd e0
    jne short 07e00h                          ; 75 9d
    mov al, byte [bp-00226h]                  ; 8a 86 da fd
    and AL, strict byte 01fh                  ; 24 1f
    cmp AL, strict byte 005h                  ; 3c 05
    je near 07d70h                            ; 0f 84 01 ff
    jmp short 07e00h                          ; eb 8f
    lea sp, [bp-00ah]                         ; 8d 66 f6
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
_scsi_init:                                  ; 0xf7e7b LB 0x66
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 e5 97
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
    jne short 07eabh                          ; 75 0c
    xor al, al                                ; 30 c0
    mov dx, 00433h                            ; ba 33 04
    out DX, AL                                ; ee
    mov ax, 00430h                            ; b8 30 04
    call 07a96h                               ; e8 eb fb
    mov AL, strict byte 055h                  ; b0 55
    mov dx, 00436h                            ; ba 36 04
    out DX, AL                                ; ee
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp AL, strict byte 055h                  ; 3c 55
    jne short 07ec4h                          ; 75 0c
    xor al, al                                ; 30 c0
    mov dx, 00437h                            ; ba 37 04
    out DX, AL                                ; ee
    mov ax, 00434h                            ; b8 34 04
    call 07a96h                               ; e8 d2 fb
    mov AL, strict byte 055h                  ; b0 55
    mov dx, 0043ah                            ; ba 3a 04
    out DX, AL                                ; ee
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp AL, strict byte 055h                  ; 3c 55
    jne short 07eddh                          ; 75 0c
    xor al, al                                ; 30 c0
    mov dx, 0043bh                            ; ba 3b 04
    out DX, AL                                ; ee
    mov ax, 00438h                            ; b8 38 04
    call 07a96h                               ; e8 b9 fb
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    retn                                      ; c3
high_bits_save_:                             ; 0xf7ee1 LB 0x17
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    mov bx, ax                                ; 89 c3
    shr eax, 010h                             ; 66 c1 e8 10
    mov es, dx                                ; 8e c2
    mov word [es:bx+00268h], ax               ; 26 89 87 68 02
    lea sp, [bp-002h]                         ; 8d 66 fe
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
high_bits_restore_:                          ; 0xf7ef8 LB 0x17
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    mov bx, ax                                ; 89 c3
    mov es, dx                                ; 8e c2
    mov ax, word [es:bx+00268h]               ; 26 8b 87 68 02
    sal eax, 010h                             ; 66 c1 e0 10
    lea sp, [bp-002h]                         ; 8d 66 fe
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
ahci_ctrl_set_bits_:                         ; 0xf7f0f LB 0x43
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    push di                                   ; 57
    push ax                                   ; 50
    mov si, ax                                ; 89 c6
    mov ax, dx                                ; 89 d0
    mov word [bp-006h], bx                    ; 89 5e fa
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
    or ax, word [bp-006h]                     ; 0b 46 fa
    mov cx, dx                                ; 89 d1
    or cx, di                                 ; 09 f9
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn                                      ; c3
ahci_ctrl_clear_bits_:                       ; 0xf7f52 LB 0x47
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    push di                                   ; 57
    push ax                                   ; 50
    mov si, ax                                ; 89 c6
    mov ax, dx                                ; 89 d0
    mov di, bx                                ; 89 df
    mov word [bp-006h], cx                    ; 89 4e fa
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
    mov cx, word [bp-006h]                    ; 8b 4e fa
    not cx                                    ; f7 d1
    and ax, di                                ; 21 f8
    and cx, dx                                ; 21 d1
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn                                      ; c3
ahci_ctrl_is_bit_set_:                       ; 0xf7f99 LB 0x39
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    push di                                   ; 57
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
    jne short 07fc5h                          ; 75 04
    test ax, bx                               ; 85 d8
    je short 07fc9h                           ; 74 04
    mov AL, strict byte 001h                  ; b0 01
    jmp short 07fcbh                          ; eb 02
    xor al, al                                ; 30 c0
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn                                      ; c3
ahci_ctrl_extract_bits_:                     ; 0xf7fd2 LB 0x1b
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov si, bx                                ; 89 de
    and ax, bx                                ; 21 d8
    and dx, cx                                ; 21 ca
    movzx cx, byte [bp+006h]                  ; 0f b6 4e 06
    jcxz 07fe8h                               ; e3 06
    shr dx, 1                                 ; d1 ea
    rcr ax, 1                                 ; d1 d8
    loop 07fe2h                               ; e2 fa
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn 00002h                               ; c2 02 00
ahci_addr_to_phys_:                          ; 0xf7fed LB 0x1e
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
    loop 07ffbh                               ; e2 fa
    xor cx, cx                                ; 31 c9
    add ax, bx                                ; 01 d8
    adc dx, cx                                ; 11 ca
    pop bp                                    ; 5d
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
ahci_port_cmd_sync_:                         ; 0xf800b LB 0xd5
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    sub sp, strict byte 00006h                ; 83 ec 06
    mov si, ax                                ; 89 c6
    mov word [bp-00ah], dx                    ; 89 56 f6
    mov es, dx                                ; 8e c2
    mov al, byte [es:si+00262h]               ; 26 8a 84 62 02
    mov byte [bp-008h], al                    ; 88 46 f8
    mov di, word [es:si+00260h]               ; 26 8b bc 60 02
    cmp AL, strict byte 0ffh                  ; 3c ff
    je near 080d8h                            ; 0f 84 aa 00
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
    call 07fedh                               ; e8 96 ff
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov word [es:si+008h], ax                 ; 26 89 44 08
    mov word [es:si+00ah], dx                 ; 26 89 54 0a
    movzx si, byte [bp-008h]                  ; 0f b6 76 f8
    sal si, 007h                              ; c1 e6 07
    lea dx, [si+00118h]                       ; 8d 94 18 01
    mov bx, strict word 00011h                ; bb 11 00
    xor cx, cx                                ; 31 c9
    mov ax, di                                ; 89 f8
    call 07f0fh                               ; e8 98 fe
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
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    sal ax, 007h                              ; c1 e0 07
    mov word [bp-00ch], ax                    ; 89 46 f4
    mov si, ax                                ; 89 c6
    add si, 00110h                            ; 81 c6 10 01
    mov bx, strict word 00001h                ; bb 01 00
    mov cx, 04000h                            ; b9 00 40
    mov dx, si                                ; 89 f2
    mov ax, di                                ; 89 f8
    call 07f99h                               ; e8 e2 fe
    test al, al                               ; 84 c0
    je short 0809ah                           ; 74 df
    mov bx, strict word 00001h                ; bb 01 00
    xor cx, cx                                ; 31 c9
    mov dx, si                                ; 89 f2
    mov ax, di                                ; 89 f8
    call 07f0fh                               ; e8 48 fe
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    add dx, 00118h                            ; 81 c2 18 01
    mov bx, strict word 00001h                ; bb 01 00
    xor cx, cx                                ; 31 c9
    mov ax, di                                ; 89 f8
    call 07f52h                               ; e8 7a fe
    lea sp, [bp-006h]                         ; 8d 66 fa
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    pop bp                                    ; 5d
    retn                                      ; c3
ahci_cmd_data_:                              ; 0xf80e0 LB 0x1ca
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    sub sp, strict byte 00010h                ; 83 ec 10
    mov di, ax                                ; 89 c7
    mov word [bp-012h], dx                    ; 89 56 ee
    mov byte [bp-008h], bl                    ; 88 5e f8
    xor si, si                                ; 31 f6
    mov es, dx                                ; 8e c2
    mov ax, word [es:di+001eeh]               ; 26 8b 85 ee 01
    mov word [bp-00ah], ax                    ; 89 46 f6
    mov word [bp-00eh], si                    ; 89 76 f2
    mov word [bp-00ch], ax                    ; 89 46 f4
    mov ax, word [es:di+00ah]                 ; 26 8b 45 0a
    mov word [bp-010h], ax                    ; 89 46 f0
    mov ax, word [es:di+00ch]                 ; 26 8b 45 0c
    mov word [bp-016h], ax                    ; 89 46 ea
    mov cx, strict word 00040h                ; b9 40 00
    xor bx, bx                                ; 31 db
    mov ax, 00080h                            ; b8 80 00
    mov dx, word [bp-00ah]                    ; 8b 56 f6
    call 094dah                               ; e8 bb 13
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov word [es:si+00080h], 08027h           ; 26 c7 84 80 00 27 80
    mov al, byte [bp-008h]                    ; 8a 46 f8
    mov byte [es:si+00082h], al               ; 26 88 84 82 00
    mov byte [es:si+00083h], 000h             ; 26 c6 84 83 00 00
    mov es, [bp-012h]                         ; 8e 46 ee
    mov al, byte [es:di]                      ; 26 8a 05
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov byte [es:si+00084h], al               ; 26 88 84 84 00
    mov es, [bp-012h]                         ; 8e 46 ee
    mov ax, word [es:di]                      ; 26 8b 05
    mov bx, word [es:di+002h]                 ; 26 8b 5d 02
    mov cx, strict word 00008h                ; b9 08 00
    shr bx, 1                                 ; d1 eb
    rcr ax, 1                                 ; d1 d8
    loop 08152h                               ; e2 fa
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov byte [es:si+00085h], al               ; 26 88 84 85 00
    mov es, [bp-012h]                         ; 8e 46 ee
    mov ax, word [es:di+002h]                 ; 26 8b 45 02
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov byte [es:si+00086h], al               ; 26 88 84 86 00
    mov byte [es:si+00087h], 040h             ; 26 c6 84 87 00 40
    mov es, [bp-012h]                         ; 8e 46 ee
    mov ax, word [es:di+002h]                 ; 26 8b 45 02
    shr ax, 008h                              ; c1 e8 08
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov byte [es:si+00088h], al               ; 26 88 84 88 00
    mov word [es:si+00089h], strict word 00000h ; 26 c7 84 89 00 00 00
    mov byte [es:si+0008bh], 000h             ; 26 c6 84 8b 00 00
    mov al, byte [bp-010h]                    ; 8a 46 f0
    mov byte [es:si+0008ch], al               ; 26 88 84 8c 00
    mov ax, word [bp-010h]                    ; 8b 46 f0
    shr ax, 008h                              ; c1 e8 08
    mov byte [es:si+0008dh], al               ; 26 88 84 8d 00
    mov word [es:si+00276h], strict word 00010h ; 26 c7 84 76 02 10 00
    mov ax, word [bp-010h]                    ; 8b 46 f0
    xor dx, dx                                ; 31 d2
    mov bx, word [bp-016h]                    ; 8b 5e ea
    xor cx, cx                                ; 31 c9
    call 094a9h                               ; e8 ee 12
    push dx                                   ; 52
    push ax                                   ; 50
    mov es, [bp-012h]                         ; 8e 46 ee
    mov bx, word [es:di+004h]                 ; 26 8b 5d 04
    mov cx, word [es:di+006h]                 ; 26 8b 4d 06
    mov ax, 0026ah                            ; b8 6a 02
    mov dx, word [bp-00ah]                    ; 8b 56 f6
    call 093c5h                               ; e8 f4 11
    mov es, [bp-00ah]                         ; 8e 46 f6
    movzx ax, byte [es:si+00263h]             ; 26 0f b6 84 63 02
    mov dx, word [es:si+0027eh]               ; 26 8b 94 7e 02
    add dx, strict byte 0ffffh                ; 83 c2 ff
    mov bx, word [es:si+00280h]               ; 26 8b 9c 80 02
    adc bx, strict byte 0ffffh                ; 83 d3 ff
    mov word [bp-014h], bx                    ; 89 5e ec
    mov bx, ax                                ; 89 c3
    sal bx, 004h                              ; c1 e3 04
    mov word [es:bx+0010ch], dx               ; 26 89 97 0c 01
    mov dx, word [bp-014h]                    ; 8b 56 ec
    mov word [es:bx+0010eh], dx               ; 26 89 97 0e 01
    mov cx, word [es:si+0027ah]               ; 26 8b 8c 7a 02
    mov dx, word [es:si+0027ch]               ; 26 8b 94 7c 02
    mov word [es:bx+00100h], cx               ; 26 89 8f 00 01
    mov word [es:bx+00102h], dx               ; 26 89 97 02 01
    inc ax                                    ; 40
    mov es, [bp-012h]                         ; 8e 46 ee
    cmp word [es:di+01ch], strict byte 00000h ; 26 83 7d 1c 00
    je short 0824ah                           ; 74 2c
    mov dx, word [es:di+01ch]                 ; 26 8b 55 1c
    dec dx                                    ; 4a
    mov di, ax                                ; 89 c7
    sal di, 004h                              ; c1 e7 04
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov word [es:di+0010ch], dx               ; 26 89 95 0c 01
    mov word [es:di+0010eh], si               ; 26 89 b5 0e 01
    mov dx, word [es:si+00264h]               ; 26 8b 94 64 02
    mov bx, word [es:si+00266h]               ; 26 8b 9c 66 02
    mov word [es:di+00100h], dx               ; 26 89 95 00 01
    mov word [es:di+00102h], bx               ; 26 89 9d 02 01
    inc ax                                    ; 40
    les bx, [bp-00eh]                         ; c4 5e f2
    mov byte [es:bx+00263h], al               ; 26 88 87 63 02
    xor ax, ax                                ; 31 c0
    les bx, [bp-00eh]                         ; c4 5e f2
    movzx dx, byte [es:bx+00263h]             ; 26 0f b6 97 63 02
    cmp ax, dx                                ; 39 d0
    jnc short 08264h                          ; 73 03
    inc ax                                    ; 40
    jmp short 08254h                          ; eb f0
    mov al, byte [bp-008h]                    ; 8a 46 f8
    cmp AL, strict byte 035h                  ; 3c 35
    jne short 08271h                          ; 75 06
    mov byte [bp-008h], 040h                  ; c6 46 f8 40
    jmp short 08285h                          ; eb 14
    cmp AL, strict byte 0a0h                  ; 3c a0
    jne short 08281h                          ; 75 0c
    or byte [bp-008h], 020h                   ; 80 4e f8 20
    or byte [es:bx+00083h], 001h              ; 26 80 8f 83 00 01
    jmp short 08285h                          ; eb 04
    mov byte [bp-008h], 000h                  ; c6 46 f8 00
    or byte [bp-008h], 005h                   ; 80 4e f8 05
    movzx bx, byte [bp-008h]                  ; 0f b6 5e f8
    mov ax, word [bp-00eh]                    ; 8b 46 f2
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    call 0800bh                               ; e8 75 fd
    mov ax, word [bp-00eh]                    ; 8b 46 f2
    add ax, 0026ah                            ; 05 6a 02
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    call 0943eh                               ; e8 9c 11
    lea sp, [bp-006h]                         ; 8d 66 fa
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    pop bp                                    ; 5d
    retn                                      ; c3
ahci_port_deinit_current_:                   ; 0xf82aa LB 0x144
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    sub sp, strict byte 00006h                ; 83 ec 06
    mov di, ax                                ; 89 c7
    mov word [bp-00ch], dx                    ; 89 56 f4
    mov es, dx                                ; 8e c2
    mov si, word [es:di+00260h]               ; 26 8b b5 60 02
    mov al, byte [es:di+00262h]               ; 26 8a 85 62 02
    mov byte [bp-00ah], al                    ; 88 46 f6
    cmp AL, strict byte 0ffh                  ; 3c ff
    je near 083e5h                            ; 0f 84 17 01
    movzx dx, al                              ; 0f b6 d0
    sal dx, 007h                              ; c1 e2 07
    add dx, 00118h                            ; 81 c2 18 01
    mov bx, strict word 00011h                ; bb 11 00
    xor cx, cx                                ; 31 c9
    mov ax, si                                ; 89 f0
    call 07f52h                               ; e8 70 fc
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6
    sal ax, 007h                              ; c1 e0 07
    mov word [bp-00eh], ax                    ; 89 46 f2
    mov dx, ax                                ; 89 c2
    add dx, 00118h                            ; 81 c2 18 01
    mov bx, 0c011h                            ; bb 11 c0
    xor cx, cx                                ; 31 c9
    mov ax, si                                ; 89 f0
    call 07f99h                               ; e8 9d fc
    cmp AL, strict byte 001h                  ; 3c 01
    je short 082e2h                           ; 74 e2
    mov cx, strict word 00020h                ; b9 20 00
    xor bx, bx                                ; 31 db
    mov ax, di                                ; 89 f8
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    call 094dah                               ; e8 cd 11
    lea ax, [di+00080h]                       ; 8d 85 80 00
    mov cx, strict word 00040h                ; b9 40 00
    xor bx, bx                                ; 31 db
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    call 094dah                               ; e8 be 11
    lea ax, [di+00200h]                       ; 8d 85 00 02
    mov cx, strict word 00060h                ; b9 60 00
    xor bx, bx                                ; 31 db
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    call 094dah                               ; e8 af 11
    mov ax, word [bp-00eh]                    ; 8b 46 f2
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
    mov ax, word [bp-00eh]                    ; 8b 46 f2
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
    mov ax, word [bp-00eh]                    ; 8b 46 f2
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
    mov ax, word [bp-00eh]                    ; 8b 46 f2
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
    mov ax, word [bp-00eh]                    ; 8b 46 f2
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
    mov es, [bp-00ch]                         ; 8e 46 f4
    mov byte [es:di+00262h], 0ffh             ; 26 c6 85 62 02 ff
    lea sp, [bp-008h]                         ; 8d 66 f8
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
ahci_port_init_:                             ; 0xf83ee LB 0x206
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    sub sp, strict byte 00006h                ; 83 ec 06
    mov si, ax                                ; 89 c6
    mov word [bp-00ah], dx                    ; 89 56 f6
    mov byte [bp-008h], bl                    ; 88 5e f8
    call 082aah                               ; e8 a8 fe
    movzx dx, bl                              ; 0f b6 d3
    sal dx, 007h                              ; c1 e2 07
    add dx, 00118h                            ; 81 c2 18 01
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov ax, word [es:si+00260h]               ; 26 8b 84 60 02
    mov bx, strict word 00011h                ; bb 11 00
    xor cx, cx                                ; 31 c9
    call 07f52h                               ; e8 36 fb
    movzx di, byte [bp-008h]                  ; 0f b6 7e f8
    sal di, 007h                              ; c1 e7 07
    lea dx, [di+00118h]                       ; 8d 95 18 01
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov ax, word [es:si+00260h]               ; 26 8b 84 60 02
    mov bx, 0c011h                            ; bb 11 c0
    xor cx, cx                                ; 31 c9
    call 07f99h                               ; e8 62 fb
    cmp AL, strict byte 001h                  ; 3c 01
    je short 0841ch                           ; 74 e1
    mov cx, strict word 00020h                ; b9 20 00
    xor bx, bx                                ; 31 db
    mov ax, si                                ; 89 f0
    mov dx, word [bp-00ah]                    ; 8b 56 f6
    call 094dah                               ; e8 92 10
    lea ax, [si+00080h]                       ; 8d 84 80 00
    mov cx, strict word 00040h                ; b9 40 00
    xor bx, bx                                ; 31 db
    mov dx, word [bp-00ah]                    ; 8b 56 f6
    call 094dah                               ; e8 83 10
    mov ax, si                                ; 89 f0
    add ah, 002h                              ; 80 c4 02
    mov word [bp-00ch], ax                    ; 89 46 f4
    mov cx, strict word 00060h                ; b9 60 00
    xor bx, bx                                ; 31 db
    mov dx, word [bp-00ah]                    ; 8b 56 f6
    call 094dah                               ; e8 70 10
    lea ax, [di+00108h]                       ; 8d 85 08 01
    cwd                                       ; 99
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov bx, word [es:si+00260h]               ; 26 8b 9c 60 02
    mov cx, dx                                ; 89 d1
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov ax, word [bp-00ch]                    ; 8b 46 f4
    mov dx, word [bp-00ah]                    ; 8b 56 f6
    call 07fedh                               ; e8 60 fb
    mov es, [bp-00ah]                         ; 8e 46 f6
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
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov bx, word [es:si+00260h]               ; 26 8b 9c 60 02
    mov cx, dx                                ; 89 d1
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov es, [bp-00ah]                         ; 8e 46 f6
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
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov bx, word [es:si+00260h]               ; 26 8b 9c 60 02
    mov cx, dx                                ; 89 d1
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov ax, si                                ; 89 f0
    mov dx, word [bp-00ah]                    ; 8b 56 f6
    call 07fedh                               ; e8 f4 fa
    mov es, [bp-00ah]                         ; 8e 46 f6
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
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov bx, word [es:si+00260h]               ; 26 8b 9c 60 02
    mov cx, dx                                ; 89 d1
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov es, [bp-00ah]                         ; 8e 46 f6
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
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov bx, word [es:si+00260h]               ; 26 8b 9c 60 02
    mov cx, dx                                ; 89 d1
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov es, [bp-00ah]                         ; 8e 46 f6
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
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov bx, word [es:si+00260h]               ; 26 8b 9c 60 02
    mov cx, dx                                ; 89 d1
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov es, [bp-00ah]                         ; 8e 46 f6
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
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov bx, word [es:si+00260h]               ; 26 8b 9c 60 02
    mov cx, dx                                ; 89 d1
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov dx, word [es:si+00260h]               ; 26 8b 94 60 02
    add dx, strict byte 00004h                ; 83 c2 04
    mov ax, strict word 0ffffh                ; b8 ff ff
    mov cx, ax                                ; 89 c1
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov al, byte [bp-008h]                    ; 8a 46 f8
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov byte [es:si+00262h], al               ; 26 88 84 62 02
    mov byte [es:si+00263h], 000h             ; 26 c6 84 63 02 00
    lea sp, [bp-006h]                         ; 8d 66 fa
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    pop bp                                    ; 5d
    retn                                      ; c3
@ahci_read_sectors:                          ; 0xf85f4 LB 0x94
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    push di                                   ; 57
    les di, [bp+004h]                         ; c4 7e 04
    movzx di, byte [es:di+008h]               ; 26 0f b6 7d 08
    sub di, strict byte 0000ch                ; 83 ef 0c
    cmp di, strict byte 00004h                ; 83 ff 04
    jbe short 08618h                          ; 76 0f
    push di                                   ; 57
    push 00c4eh                               ; 68 4e 0c
    push 00c60h                               ; 68 60 0c
    push strict byte 00007h                   ; 6a 07
    call 01972h                               ; e8 5d 93
    add sp, strict byte 00008h                ; 83 c4 08
    les bx, [bp+004h]                         ; c4 5e 04
    mov dx, word [es:bx+001eeh]               ; 26 8b 97 ee 01
    xor ax, ax                                ; 31 c0
    call 07ee1h                               ; e8 bc f8
    mov es, [bp+006h]                         ; 8e 46 06
    add di, bx                                ; 01 df
    movzx bx, byte [es:di+001e9h]             ; 26 0f b6 9d e9 01
    mov di, word [bp+004h]                    ; 8b 7e 04
    mov dx, word [es:di+001eeh]               ; 26 8b 95 ee 01
    xor ax, ax                                ; 31 c0
    call 083eeh                               ; e8 b1 fd
    mov bx, strict word 00025h                ; bb 25 00
    mov ax, di                                ; 89 f8
    mov dx, word [bp+006h]                    ; 8b 56 06
    call 080e0h                               ; e8 98 fa
    mov es, [bp+006h]                         ; 8e 46 06
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
    mov es, [bp+006h]                         ; 8e 46 06
    mov dx, word [es:bx+001eeh]               ; 26 8b 97 ee 01
    xor ax, ax                                ; 31 c0
    call 07ef8h                               ; e8 7b f8
    xor ax, ax                                ; 31 c0
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn 00004h                               ; c2 04 00
@ahci_write_sectors:                         ; 0xf8688 LB 0x70
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    mov si, word [bp+004h]                    ; 8b 76 04
    mov cx, word [bp+006h]                    ; 8b 4e 06
    mov es, cx                                ; 8e c1
    movzx bx, byte [es:si+008h]               ; 26 0f b6 5c 08
    sub bx, strict byte 0000ch                ; 83 eb 0c
    cmp bx, strict byte 00004h                ; 83 fb 04
    jbe short 086b0h                          ; 76 0f
    push bx                                   ; 53
    push 00c7fh                               ; 68 7f 0c
    push 00c60h                               ; 68 60 0c
    push strict byte 00007h                   ; 6a 07
    call 01972h                               ; e8 c5 92
    add sp, strict byte 00008h                ; 83 c4 08
    mov es, cx                                ; 8e c1
    mov dx, word [es:si+001eeh]               ; 26 8b 94 ee 01
    xor ax, ax                                ; 31 c0
    call 07ee1h                               ; e8 25 f8
    mov es, cx                                ; 8e c1
    add bx, si                                ; 01 f3
    movzx bx, byte [es:bx+001e9h]             ; 26 0f b6 9f e9 01
    mov dx, word [es:si+001eeh]               ; 26 8b 94 ee 01
    xor ax, ax                                ; 31 c0
    call 083eeh                               ; e8 1e fd
    mov bx, strict word 00035h                ; bb 35 00
    mov ax, si                                ; 89 f0
    mov dx, cx                                ; 89 ca
    call 080e0h                               ; e8 06 fa
    mov es, cx                                ; 8e c1
    mov dx, word [es:si+00ah]                 ; 26 8b 54 0a
    mov word [es:si+014h], dx                 ; 26 89 54 14
    mov dx, word [es:si+001eeh]               ; 26 8b 94 ee 01
    xor ax, ax                                ; 31 c0
    call 07ef8h                               ; e8 0a f8
    xor ax, ax                                ; 31 c0
    lea sp, [bp-002h]                         ; 8d 66 fe
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn 00004h                               ; c2 04 00
ahci_cmd_packet_:                            ; 0xf86f8 LB 0x173
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    push di                                   ; 57
    sub sp, strict byte 0000eh                ; 83 ec 0e
    push ax                                   ; 50
    mov byte [bp-006h], dl                    ; 88 56 fa
    mov word [bp-012h], bx                    ; 89 5e ee
    mov word [bp-010h], cx                    ; 89 4e f0
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 59 8f
    mov si, 00122h                            ; be 22 01
    mov word [bp-008h], ax                    ; 89 46 f8
    cmp byte [bp+00ah], 002h                  ; 80 7e 0a 02
    jne short 0873eh                          ; 75 1f
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 07 92
    push 00c92h                               ; 68 92 0c
    push 00ca2h                               ; 68 a2 0c
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 3d 92
    add sp, strict byte 00006h                ; 83 c4 06
    mov ax, strict word 00001h                ; b8 01 00
    jmp near 08862h                           ; e9 24 01
    test byte [bp+004h], 001h                 ; f6 46 04 01
    jne short 08738h                          ; 75 f4
    mov ax, word [bp+006h]                    ; 8b 46 06
    mov dx, word [bp+008h]                    ; 8b 56 08
    mov cx, strict word 00008h                ; b9 08 00
    sal ax, 1                                 ; d1 e0
    rcl dx, 1                                 ; d1 d2
    loop 0874dh                               ; e2 fa
    mov es, [bp-008h]                         ; 8e 46 f8
    mov word [es:si], ax                      ; 26 89 04
    mov word [es:si+002h], dx                 ; 26 89 54 02
    mov ax, word [bp+00ch]                    ; 8b 46 0c
    mov word [es:si+004h], ax                 ; 26 89 44 04
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    mov word [es:si+006h], ax                 ; 26 89 44 06
    mov bx, word [es:si+00ch]                 ; 26 8b 5c 0c
    mov ax, word [bp+006h]                    ; 8b 46 06
    mov dx, word [bp+008h]                    ; 8b 56 08
    xor cx, cx                                ; 31 c9
    call 09470h                               ; e8 f6 0c
    mov word [es:si+00ah], ax                 ; 26 89 44 0a
    xor di, di                                ; 31 ff
    mov ax, word [es:si+001eeh]               ; 26 8b 84 ee 01
    mov word [bp-00ah], ax                    ; 89 46 f6
    mov word [bp-00eh], di                    ; 89 7e f2
    mov word [bp-00ch], ax                    ; 89 46 f4
    sub word [bp-014h], strict byte 0000ch    ; 83 6e ec 0c
    xor ax, ax                                ; 31 c0
    mov dx, word [bp-00ah]                    ; 8b 56 f6
    call 07ee1h                               ; e8 47 f7
    mov es, [bp-008h]                         ; 8e 46 f8
    mov bx, word [bp-014h]                    ; 8b 5e ec
    add bx, si                                ; 01 f3
    movzx bx, byte [es:bx+001e9h]             ; 26 0f b6 9f e9 01
    mov dx, word [es:si+001eeh]               ; 26 8b 94 ee 01
    xor ax, ax                                ; 31 c0
    call 083eeh                               ; e8 3c fc
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa
    push ax                                   ; 50
    mov bx, word [bp-012h]                    ; 8b 5e ee
    mov cx, word [bp-010h]                    ; 8b 4e f0
    mov ax, 000c0h                            ; b8 c0 00
    mov dx, word [bp-00ah]                    ; 8b 56 f6
    call 094e7h                               ; e8 21 0d
    mov es, [bp-008h]                         ; 8e 46 f8
    mov word [es:si+014h], di                 ; 26 89 7c 14
    mov word [es:si+016h], di                 ; 26 89 7c 16
    mov word [es:si+018h], di                 ; 26 89 7c 18
    mov ax, word [es:si+01ah]                 ; 26 8b 44 1a
    test ax, ax                               ; 85 c0
    je short 08804h                           ; 74 27
    dec ax                                    ; 48
    mov es, [bp-00ah]                         ; 8e 46 f6
    mov word [es:di+0010ch], ax               ; 26 89 85 0c 01
    mov word [es:di+0010eh], di               ; 26 89 bd 0e 01
    mov ax, word [es:di+00264h]               ; 26 8b 85 64 02
    mov dx, word [es:di+00266h]               ; 26 8b 95 66 02
    mov word [es:di+00100h], ax               ; 26 89 85 00 01
    mov word [es:di+00102h], dx               ; 26 89 95 02 01
    inc byte [es:di+00263h]                   ; 26 fe 85 63 02
    mov bx, 000a0h                            ; bb a0 00
    mov ax, si                                ; 89 f0
    mov dx, word [bp-008h]                    ; 8b 56 f8
    call 080e0h                               ; e8 d1 f8
    les bx, [bp-00eh]                         ; c4 5e f2
    mov ax, word [es:bx+004h]                 ; 26 8b 47 04
    mov dx, word [es:bx+006h]                 ; 26 8b 57 06
    mov es, [bp-008h]                         ; 8e 46 f8
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
    mov ax, word [bp-00eh]                    ; 8b 46 f2
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    call 07ef8h                               ; e8 aa f6
    les bx, [bp-00eh]                         ; c4 5e f2
    mov ax, word [es:bx+006h]                 ; 26 8b 47 06
    or ax, word [es:bx+004h]                  ; 26 0b 47 04
    jne short 08860h                          ; 75 05
    mov ax, strict word 00004h                ; b8 04 00
    jmp short 08862h                          ; eb 02
    xor ax, ax                                ; 31 c0
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn 0000ch                               ; c2 0c 00
ahci_port_detect_device_:                    ; 0xf886b LB 0x401
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    sub sp, 00222h                            ; 81 ec 22 02
    mov di, ax                                ; 89 c7
    mov word [bp-010h], dx                    ; 89 56 f0
    mov byte [bp-00ch], bl                    ; 88 5e f4
    movzx cx, bl                              ; 0f b6 cb
    mov bx, cx                                ; 89 cb
    call 083eeh                               ; e8 69 fb
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 de 8d
    mov word [bp-020h], ax                    ; 89 46 e0
    mov si, 00122h                            ; be 22 01
    mov word [bp-00eh], ax                    ; 89 46 f2
    mov word [bp-012h], si                    ; 89 76 ee
    mov word [bp-01eh], ax                    ; 89 46 e2
    sal cx, 007h                              ; c1 e1 07
    mov word [bp-016h], cx                    ; 89 4e ea
    mov ax, cx                                ; 89 c8
    add ax, 0012ch                            ; 05 2c 01
    cwd                                       ; 99
    mov word [bp-022h], ax                    ; 89 46 de
    mov bx, dx                                ; 89 d3
    mov es, [bp-010h]                         ; 8e 46 f0
    mov dx, word [es:di+00260h]               ; 26 8b 95 60 02
    mov cx, bx                                ; 89 d9
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov es, [bp-010h]                         ; 8e 46 f0
    mov dx, word [es:di+00260h]               ; 26 8b 95 60 02
    add dx, strict byte 00004h                ; 83 c2 04
    mov ax, strict word 00001h                ; b8 01 00
    xor cx, cx                                ; 31 c9
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov es, [bp-010h]                         ; 8e 46 f0
    mov dx, word [es:di+00260h]               ; 26 8b 95 60 02
    mov ax, word [bp-022h]                    ; 8b 46 de
    mov cx, bx                                ; 89 d9
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov es, [bp-010h]                         ; 8e 46 f0
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
    mov es, [bp-010h]                         ; 8e 46 f0
    mov bx, word [es:di+00260h]               ; 26 8b 9d 60 02
    mov cx, dx                                ; 89 d1
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov es, [bp-010h]                         ; 8e 46 f0
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
    call 07fd2h                               ; e8 90 f6
    cmp ax, strict word 00003h                ; 3d 03 00
    jne near 08c64h                           ; 0f 85 1b 03
    mov ax, word [bp-016h]                    ; 8b 46 ea
    add ax, 00130h                            ; 05 30 01
    cwd                                       ; 99
    mov es, [bp-010h]                         ; 8e 46 f0
    mov bx, word [es:di+00260h]               ; 26 8b 9d 60 02
    mov cx, dx                                ; 89 d1
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov es, [bp-010h]                         ; 8e 46 f0
    mov dx, word [es:di+00260h]               ; 26 8b 95 60 02
    add dx, strict byte 00004h                ; 83 c2 04
    mov ax, strict word 0ffffh                ; b8 ff ff
    mov cx, ax                                ; 89 c1
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov es, [bp-00eh]                         ; 8e 46 f2
    mov al, byte [es:si+001edh]               ; 26 8a 84 ed 01
    mov byte [bp-008h], al                    ; 88 46 f8
    cmp AL, strict byte 004h                  ; 3c 04
    jnc near 08c64h                           ; 0f 83 d5 02
    mov dx, word [bp-016h]                    ; 8b 56 ea
    add dx, 00118h                            ; 81 c2 18 01
    mov es, [bp-010h]                         ; 8e 46 f0
    mov ax, word [es:di+00260h]               ; 26 8b 85 60 02
    mov bx, strict word 00010h                ; bb 10 00
    xor cx, cx                                ; 31 c9
    call 07f0fh                               ; e8 69 f5
    mov ax, word [bp-016h]                    ; 8b 46 ea
    add ax, 00124h                            ; 05 24 01
    cwd                                       ; 99
    mov es, [bp-010h]                         ; 8e 46 f0
    mov bx, word [es:di+00260h]               ; 26 8b 9d 60 02
    mov cx, dx                                ; 89 d1
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov es, [bp-010h]                         ; 8e 46 f0
    mov dx, word [es:di+00260h]               ; 26 8b 95 60 02
    add dx, strict byte 00004h                ; 83 c2 04
    in eax, DX                                ; 66 ed
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0
    shr eax, 010h                             ; 66 c1 e8 10
    xchg dx, ax                               ; 92
    mov cl, byte [bp-008h]                    ; 8a 4e f8
    add cl, 00ch                              ; 80 c1 0c
    test dx, dx                               ; 85 d2
    jne near 08bb6h                           ; 0f 85 d4 01
    cmp ax, 00101h                            ; 3d 01 01
    jne near 08bb6h                           ; 0f 85 cd 01
    mov es, [bp-00eh]                         ; 8e 46 f2
    db  066h, 026h, 0c7h, 004h, 000h, 000h, 000h, 000h
    ; mov dword [es:si], strict dword 000000000h ; 66 26 c7 04 00 00 00 00
    lea dx, [bp-00228h]                       ; 8d 96 d8 fd
    mov word [es:si+004h], dx                 ; 26 89 54 04
    mov [es:si+006h], ss                      ; 26 8c 54 06
    db  066h, 026h, 0c7h, 044h, 00ah, 001h, 000h, 000h, 002h
    ; mov dword [es:si+00ah], strict dword 002000001h ; 66 26 c7 44 0a 01 00 00 02
    mov bx, 000ech                            ; bb ec 00
    mov ax, si                                ; 89 f0
    mov dx, word [bp-020h]                    ; 8b 56 e0
    call 080e0h                               ; e8 cc f6
    mov byte [bp-00ah], cl                    ; 88 4e f6
    test byte [bp-00228h], 080h               ; f6 86 d8 fd 80
    db  00fh, 095h, 0c0h
    ; setne al                                  ; 0f 95 c0
    xor ah, ah                                ; 30 e4
    mov dx, word [bp-00226h]                  ; 8b 96 da fd
    mov word [bp-018h], dx                    ; 89 56 e8
    mov dx, word [bp-00222h]                  ; 8b 96 de fd
    mov word [bp-01ch], dx                    ; 89 56 e4
    mov dx, word [bp-0021ch]                  ; 8b 96 e4 fd
    mov word [bp-01ah], dx                    ; 89 56 e6
    mov di, word [bp-001b0h]                  ; 8b be 50 fe
    mov dx, word [bp-001aeh]                  ; 8b 96 52 fe
    mov word [bp-014h], dx                    ; 89 56 ec
    cmp dx, 00fffh                            ; 81 fa ff 0f
    jne short 08a57h                          ; 75 10
    cmp di, strict byte 0ffffh                ; 83 ff ff
    jne short 08a57h                          ; 75 0b
    mov di, word [bp-00160h]                  ; 8b be a0 fe
    mov dx, word [bp-0015eh]                  ; 8b 96 a2 fe
    mov word [bp-014h], dx                    ; 89 56 ec
    movzx bx, byte [bp-008h]                  ; 0f b6 5e f8
    mov es, [bp-01eh]                         ; 8e 46 e2
    add bx, word [bp-012h]                    ; 03 5e ee
    mov ah, byte [bp-00ch]                    ; 8a 66 f4
    mov byte [es:bx+001e9h], ah               ; 26 88 a7 e9 01
    movzx dx, byte [bp-00ah]                  ; 0f b6 56 f6
    imul dx, dx, strict byte 00018h           ; 6b d2 18
    mov si, word [bp-012h]                    ; 8b 76 ee
    add si, dx                                ; 01 d6
    mov word [es:si+01eh], 0ff05h             ; 26 c7 44 1e 05 ff
    mov byte [es:si+020h], al                 ; 26 88 44 20
    mov byte [es:si+021h], 000h               ; 26 c6 44 21 00
    mov word [es:si+024h], 00200h             ; 26 c7 44 24 00 02
    mov byte [es:si+023h], 001h               ; 26 c6 44 23 01
    mov word [es:si+032h], di                 ; 26 89 7c 32
    mov ax, word [bp-014h]                    ; 8b 46 ec
    mov word [es:si+034h], ax                 ; 26 89 44 34
    mov ax, word [bp-01ch]                    ; 8b 46 e4
    mov word [es:si+02ch], ax                 ; 26 89 44 2c
    mov ax, word [bp-018h]                    ; 8b 46 e8
    mov word [es:si+02eh], ax                 ; 26 89 44 2e
    mov ax, word [bp-01ah]                    ; 8b 46 e6
    mov word [es:si+030h], ax                 ; 26 89 44 30
    mov al, byte [bp-008h]                    ; 8a 46 f8
    cmp AL, strict byte 001h                  ; 3c 01
    jc short 08ac2h                           ; 72 0c
    jbe short 08acah                          ; 76 12
    cmp AL, strict byte 003h                  ; 3c 03
    je short 08ad2h                           ; 74 16
    cmp AL, strict byte 002h                  ; 3c 02
    je short 08aceh                           ; 74 0e
    jmp short 08b1bh                          ; eb 59
    test al, al                               ; 84 c0
    jne short 08b1bh                          ; 75 55
    mov DL, strict byte 040h                  ; b2 40
    jmp short 08ad4h                          ; eb 0a
    mov DL, strict byte 048h                  ; b2 48
    jmp short 08ad4h                          ; eb 06
    mov DL, strict byte 050h                  ; b2 50
    jmp short 08ad4h                          ; eb 02
    mov DL, strict byte 058h                  ; b2 58
    mov al, dl                                ; 88 d0
    add AL, strict byte 007h                  ; 04 07
    movzx bx, al                              ; 0f b6 d8
    mov ax, bx                                ; 89 d8
    call 016ach                               ; e8 cc 8b
    test al, al                               ; 84 c0
    je short 08b1bh                           ; 74 37
    mov al, dl                                ; 88 d0
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    xor ah, ah                                ; 30 e4
    call 016ach                               ; e8 bf 8b
    xor ah, ah                                ; 30 e4
    mov cx, ax                                ; 89 c1
    sal cx, 008h                              ; c1 e1 08
    movzx ax, dl                              ; 0f b6 c2
    call 016ach                               ; e8 b2 8b
    xor ah, ah                                ; 30 e4
    add ax, cx                                ; 01 c8
    mov word [bp-026h], ax                    ; 89 46 da
    mov al, dl                                ; 88 d0
    add AL, strict byte 002h                  ; 04 02
    xor ah, ah                                ; 30 e4
    call 016ach                               ; e8 a2 8b
    xor ah, ah                                ; 30 e4
    mov word [bp-028h], ax                    ; 89 46 d8
    mov ax, bx                                ; 89 d8
    call 016ach                               ; e8 98 8b
    xor ah, ah                                ; 30 e4
    mov word [bp-024h], ax                    ; 89 46 dc
    jmp short 08b28h                          ; eb 0d
    mov bx, di                                ; 89 fb
    mov cx, word [bp-014h]                    ; 8b 4e ec
    mov dx, ss                                ; 8c d2
    lea ax, [bp-028h]                         ; 8d 46 d8
    call 05389h                               ; e8 61 c8
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 fe 8d
    push word [bp-014h]                       ; ff 76 ec
    push di                                   ; 57
    mov ax, word [bp-024h]                    ; 8b 46 dc
    push ax                                   ; 50
    mov ax, word [bp-028h]                    ; 8b 46 d8
    push ax                                   ; 50
    mov ax, word [bp-026h]                    ; 8b 46 da
    push ax                                   ; 50
    push dword [bp-01ch]                      ; 66 ff 76 e4
    push word [bp-018h]                       ; ff 76 e8
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4
    push ax                                   ; 50
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    push ax                                   ; 50
    push 00cc2h                               ; 68 c2 0c
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 16 8e
    add sp, strict byte 00018h                ; 83 c4 18
    movzx di, byte [bp-00ah]                  ; 0f b6 7e f6
    imul di, di, strict byte 00018h           ; 6b ff 18
    add di, word [bp-012h]                    ; 03 7e ee
    mov es, [bp-01eh]                         ; 8e 46 e2
    lea di, [di+026h]                         ; 8d 7d 26
    push DS                                   ; 1e
    push SS                                   ; 16
    pop DS                                    ; 1f
    lea si, [bp-028h]                         ; 8d 76 d8
    movsw                                     ; a5
    movsw                                     ; a5
    movsw                                     ; a5
    pop DS                                    ; 1f
    mov bx, word [bp-012h]                    ; 8b 5e ee
    mov al, byte [es:bx+0019eh]               ; 26 8a 87 9e 01
    mov ah, byte [bp-008h]                    ; 8a 66 f8
    add ah, 00ch                              ; 80 c4 0c
    movzx bx, al                              ; 0f b6 d8
    add bx, word [bp-012h]                    ; 03 5e ee
    mov byte [es:bx+0019fh], ah               ; 26 88 a7 9f 01
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    mov bx, word [bp-012h]                    ; 8b 5e ee
    mov byte [es:bx+0019eh], al               ; 26 88 87 9e 01
    mov dx, strict word 00075h                ; ba 75 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01650h                               ; e8 ab 8a
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 00075h                ; ba 75 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0165eh                               ; e8 ab 8a
    jmp near 08c53h                           ; e9 9d 00
    cmp dx, 0eb14h                            ; 81 fa 14 eb
    jne near 08c53h                           ; 0f 85 95 00
    cmp ax, 00101h                            ; 3d 01 01
    jne near 08c53h                           ; 0f 85 8e 00
    mov es, [bp-00eh]                         ; 8e 46 f2
    db  066h, 026h, 0c7h, 004h, 000h, 000h, 000h, 000h
    ; mov dword [es:si], strict dword 000000000h ; 66 26 c7 04 00 00 00 00
    lea dx, [bp-00228h]                       ; 8d 96 d8 fd
    mov word [es:si+004h], dx                 ; 26 89 54 04
    mov [es:si+006h], ss                      ; 26 8c 54 06
    db  066h, 026h, 0c7h, 044h, 00ah, 001h, 000h, 000h, 002h
    ; mov dword [es:si+00ah], strict dword 002000001h ; 66 26 c7 44 0a 01 00 00 02
    mov bx, 000a1h                            ; bb a1 00
    mov ax, si                                ; 89 f0
    mov dx, word [bp-020h]                    ; 8b 56 e0
    call 080e0h                               ; e8 f0 f4
    test byte [bp-00228h], 080h               ; f6 86 d8 fd 80
    db  00fh, 095h, 0c0h
    ; setne al                                  ; 0f 95 c0
    movzx dx, al                              ; 0f b6 d0
    movzx bx, byte [bp-008h]                  ; 0f b6 5e f8
    mov es, [bp-020h]                         ; 8e 46 e0
    add bx, si                                ; 01 f3
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    mov byte [es:bx+001e9h], al               ; 26 88 87 e9 01
    movzx si, cl                              ; 0f b6 f1
    imul si, si, strict byte 00018h           ; 6b f6 18
    add si, 00122h                            ; 81 c6 22 01
    mov word [es:si+01eh], 00505h             ; 26 c7 44 1e 05 05
    mov byte [es:si+020h], dl                 ; 26 88 54 20
    mov word [es:si+024h], 00800h             ; 26 c7 44 24 00 08
    mov es, [bp-01eh]                         ; 8e 46 e2
    mov bx, word [bp-012h]                    ; 8b 5e ee
    mov al, byte [es:bx+001afh]               ; 26 8a 87 af 01
    mov ah, byte [bp-008h]                    ; 8a 66 f8
    add ah, 00ch                              ; 80 c4 0c
    movzx bx, al                              ; 0f b6 d8
    mov es, [bp-020h]                         ; 8e 46 e0
    add bx, 00122h                            ; 81 c3 22 01
    mov byte [es:bx+001b0h], ah               ; 26 88 a7 b0 01
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    mov es, [bp-01eh]                         ; 8e 46 e2
    mov bx, word [bp-012h]                    ; 8b 5e ee
    mov byte [es:bx+001afh], al               ; 26 88 87 af 01
    inc byte [bp-008h]                        ; fe 46 f8
    mov al, byte [bp-008h]                    ; 8a 46 f8
    mov es, [bp-01eh]                         ; 8e 46 e2
    mov bx, word [bp-012h]                    ; 8b 5e ee
    mov byte [es:bx+001edh], al               ; 26 88 87 ed 01
    lea sp, [bp-006h]                         ; 8d 66 fa
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    pop bp                                    ; 5d
    retn                                      ; c3
ahci_mem_alloc_:                             ; 0xf8c6c LB 0x43
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push si                                   ; 56
    push di                                   ; 57
    mov dx, 00413h                            ; ba 13 04
    xor ax, ax                                ; 31 c0
    call 0166ch                               ; e8 f0 89
    test ax, ax                               ; 85 c0
    je short 08ca5h                           ; 74 25
    dec ax                                    ; 48
    mov bx, ax                                ; 89 c3
    xor dx, dx                                ; 31 d2
    mov cx, strict word 0000ah                ; b9 0a 00
    sal ax, 1                                 ; d1 e0
    rcl dx, 1                                 ; d1 d2
    loop 08c88h                               ; e2 fa
    mov si, ax                                ; 89 c6
    mov di, dx                                ; 89 d7
    mov cx, strict word 00004h                ; b9 04 00
    shr di, 1                                 ; d1 ef
    rcr si, 1                                 ; d1 de
    loop 08c95h                               ; e2 fa
    mov dx, 00413h                            ; ba 13 04
    xor ax, ax                                ; 31 c0
    call 0167ah                               ; e8 d7 89
    mov ax, si                                ; 89 f0
    lea sp, [bp-00ah]                         ; 8d 66 f6
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
ahci_hba_init_:                              ; 0xf8caf LB 0x125
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push si                                   ; 56
    push di                                   ; 57
    sub sp, strict byte 00006h                ; 83 ec 06
    mov si, ax                                ; 89 c6
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0166ch                               ; e8 a7 89
    mov bx, 00122h                            ; bb 22 01
    mov word [bp-010h], ax                    ; 89 46 f0
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
    call 08c6ch                               ; e8 82 ff
    mov di, ax                                ; 89 c7
    test ax, ax                               ; 85 c0
    je near 08db3h                            ; 0f 84 c1 00
    mov es, [bp-010h]                         ; 8e 46 f0
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
    call 07f0fh                               ; e8 e9 f1
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
    jne short 08d26h                          ; 75 de
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
    call 07fd2h                               ; e8 66 f2
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    mov byte [bp-00eh], al                    ; 88 46 f2
    mov byte [bp-00ch], 000h                  ; c6 46 f4 00
    jmp short 08d80h                          ; eb 09
    inc byte [bp-00ch]                        ; fe 46 f4
    cmp byte [bp-00ch], 020h                  ; 80 7e f4 20
    jnc short 08db1h                          ; 73 31
    movzx cx, byte [bp-00ch]                  ; 0f b6 4e f4
    mov ax, strict word 00001h                ; b8 01 00
    xor dx, dx                                ; 31 d2
    jcxz 08d91h                               ; e3 06
    sal ax, 1                                 ; d1 e0
    rcl dx, 1                                 ; d1 d2
    loop 08d8bh                               ; e2 fa
    mov bx, ax                                ; 89 c3
    mov cx, dx                                ; 89 d1
    mov dx, strict word 0000ch                ; ba 0c 00
    mov ax, si                                ; 89 f0
    call 07f99h                               ; e8 fc f1
    test al, al                               ; 84 c0
    je short 08d77h                           ; 74 d6
    movzx bx, byte [bp-00ch]                  ; 0f b6 5e f4
    xor ax, ax                                ; 31 c0
    mov dx, di                                ; 89 fa
    call 0886bh                               ; e8 bf fa
    dec byte [bp-00eh]                        ; fe 4e f2
    jne short 08d77h                          ; 75 c6
    xor ax, ax                                ; 31 c0
    lea sp, [bp-00ah]                         ; 8d 66 f6
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
    db  00bh, 005h, 004h, 003h, 002h, 001h, 000h, 0a3h, 08eh, 081h, 08eh, 087h, 08eh, 08dh, 08eh, 093h
    db  08eh, 099h, 08eh, 09fh, 08eh, 0a3h, 08eh
_ahci_init:                                  ; 0xf8dd4 LB 0xfe
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    push di                                   ; 57
    sub sp, strict byte 00006h                ; 83 ec 06
    mov ax, 00601h                            ; b8 01 06
    mov dx, strict word 00001h                ; ba 01 00
    call 092f2h                               ; e8 0d 05
    mov dx, ax                                ; 89 c2
    cmp ax, strict word 0ffffh                ; 3d ff ff
    je near 08ecbh                            ; 0f 84 dd 00
    xor al, al                                ; 30 c0
    shr ax, 008h                              ; c1 e8 08
    mov byte [bp-00ah], al                    ; 88 46 f6
    mov byte [bp-008h], dl                    ; 88 56 f8
    xor dh, dh                                ; 30 f6
    xor ah, ah                                ; 30 e4
    mov bx, strict word 00034h                ; bb 34 00
    call 0931dh                               ; e8 1a 05
    mov cl, al                                ; 88 c1
    test cl, cl                               ; 84 c9
    je short 08e2ch                           ; 74 23
    movzx bx, cl                              ; 0f b6 d9
    movzx di, byte [bp-008h]                  ; 0f b6 7e f8
    movzx si, byte [bp-00ah]                  ; 0f b6 76 f6
    mov dx, di                                ; 89 fa
    mov ax, si                                ; 89 f0
    call 0931dh                               ; e8 02 05
    cmp AL, strict byte 012h                  ; 3c 12
    je short 08e2ch                           ; 74 0d
    mov al, cl                                ; 88 c8
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    movzx bx, al                              ; 0f b6 d8
    mov dx, di                                ; 89 fa
    mov ax, si                                ; 89 f0
    jmp short 08e00h                          ; eb d4
    test cl, cl                               ; 84 c9
    je near 08ecbh                            ; 0f 84 99 00
    add cl, 002h                              ; 80 c1 02
    movzx bx, cl                              ; 0f b6 d9
    movzx di, byte [bp-008h]                  ; 0f b6 7e f8
    movzx si, byte [bp-00ah]                  ; 0f b6 76 f6
    mov dx, di                                ; 89 fa
    mov ax, si                                ; 89 f0
    call 0931dh                               ; e8 d6 04
    cmp AL, strict byte 010h                  ; 3c 10
    jne near 08ecbh                           ; 0f 85 7e 00
    mov byte [bp-006h], 000h                  ; c6 46 fa 00
    mov al, cl                                ; 88 c8
    add AL, strict byte 002h                  ; 04 02
    movzx bx, al                              ; 0f b6 d8
    mov dx, di                                ; 89 fa
    mov ax, si                                ; 89 f0
    call 09341h                               ; e8 e2 04
    mov dx, ax                                ; 89 c2
    and ax, strict word 0000fh                ; 25 0f 00
    sub ax, strict word 00004h                ; 2d 04 00
    cmp ax, strict word 0000bh                ; 3d 0b 00
    jnbe short 08ea3h                         ; 77 37
    push CS                                   ; 0e
    pop ES                                    ; 07
    mov cx, strict word 00008h                ; b9 08 00
    mov di, 08dbdh                            ; bf bd 8d
    repne scasb                               ; f2 ae
    sal cx, 1                                 ; d1 e1
    mov di, cx                                ; 89 cf
    mov ax, word [cs:di-0723ch]               ; 2e 8b 85 c4 8d
    jmp ax                                    ; ff e0
    mov byte [bp-006h], 010h                  ; c6 46 fa 10
    jmp short 08ea3h                          ; eb 1c
    mov byte [bp-006h], 014h                  ; c6 46 fa 14
    jmp short 08ea3h                          ; eb 16
    mov byte [bp-006h], 018h                  ; c6 46 fa 18
    jmp short 08ea3h                          ; eb 10
    mov byte [bp-006h], 01ch                  ; c6 46 fa 1c
    jmp short 08ea3h                          ; eb 0a
    mov byte [bp-006h], 020h                  ; c6 46 fa 20
    jmp short 08ea3h                          ; eb 04
    mov byte [bp-006h], 024h                  ; c6 46 fa 24
    mov si, dx                                ; 89 d6
    shr si, 004h                              ; c1 ee 04
    sal si, 002h                              ; c1 e6 02
    mov al, byte [bp-006h]                    ; 8a 46 fa
    test al, al                               ; 84 c0
    je short 08ecbh                           ; 74 19
    movzx bx, al                              ; 0f b6 d8
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6
    call 09363h                               ; e8 a3 04
    test AL, strict byte 001h                 ; a8 01
    je short 08ecbh                           ; 74 07
    and AL, strict byte 0f0h                  ; 24 f0
    add ax, si                                ; 01 f0
    call 08cafh                               ; e8 e4 fd
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn                                      ; c3
apm_out_str_:                                ; 0xf8ed2 LB 0x39
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    mov bx, ax                                ; 89 c3
    cmp byte [bx], 000h                       ; 80 3f 00
    je short 08ee7h                           ; 74 0a
    mov al, byte [bx]                         ; 8a 07
    out DX, AL                                ; ee
    inc bx                                    ; 43
    mov al, byte [bx]                         ; 8a 07
    db  00ah, 0c0h
    ; or al, al                                 ; 0a c0
    jne short 08edfh                          ; 75 f8
    lea sp, [bp-002h]                         ; 8d 66 fe
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
    db  02eh, 08fh, 0fah
    ; cs pop dx                                 ; 2e 8f fa
    pop word [bx+si-071h]                     ; 8f 40 8f
    pop bx                                    ; 5b
    db  08fh, 0fah
    ; pop dx                                    ; 8f fa
    pop word [bp-00571h]                      ; 8f 86 8f fa
    db  08fh, 08bh, 08fh, 0cfh
    ; pop word [bp+di-03071h]                   ; 8f 8b 8f cf
    db  08fh, 0cfh
    ; pop di                                    ; 8f cf
    db  08fh, 0cfh
    ; pop di                                    ; 8f cf
    db  08fh, 0cah
    ; pop dx                                    ; 8f ca
    db  08fh, 0cfh
    ; pop di                                    ; 8f cf
    db  08fh, 0cfh
    ; pop di                                    ; 8f cf
    db  08fh, 0c3h
    ; pop bx                                    ; 8f c3
    db  08fh
_apm_function:                               ; 0xf8f0b LB 0xf5
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    and byte [bp+018h], 0feh                  ; 80 66 18 fe
    mov ax, word [bp+012h]                    ; 8b 46 12
    xor ah, ah                                ; 30 e4
    cmp ax, strict word 0000eh                ; 3d 0e 00
    jnbe near 08fcfh                          ; 0f 87 b0 00
    mov bx, ax                                ; 89 c3
    add bx, ax                                ; 01 c3
    mov dx, word [bp+018h]                    ; 8b 56 18
    or dl, 001h                               ; 80 ca 01
    jmp word [cs:bx-07113h]                   ; 2e ff a7 ed 8e
    mov word [bp+012h], 00102h                ; c7 46 12 02 01
    mov word [bp+00ch], 0504dh                ; c7 46 0c 4d 50
    mov word [bp+010h], strict word 00003h    ; c7 46 10 03 00
    jmp near 08ffah                           ; e9 ba 00
    mov word [bp+012h], 0f000h                ; c7 46 12 00 f0
    mov word [bp+00ch], 09554h                ; c7 46 0c 54 95
    mov word [bp+010h], 0f000h                ; c7 46 10 00 f0
    mov ax, strict word 0fff0h                ; b8 f0 ff
    mov word [bp+006h], ax                    ; 89 46 06
    mov word [bp+004h], ax                    ; 89 46 04
    jmp near 08ffah                           ; e9 9f 00
    mov word [bp+012h], 0f000h                ; c7 46 12 00 f0
    mov word [bp+00ch], 0da40h                ; c7 46 0c 40 da
    mov ax, 0f000h                            ; b8 00 f0
    mov word [bp+010h], ax                    ; 89 46 10
    mov word [bp+00eh], ax                    ; 89 46 0e
    mov ax, strict word 0fff0h                ; b8 f0 ff
    mov word [bp+006h], ax                    ; 89 46 06
    mov word [bp+004h], ax                    ; 89 46 04
    xor bx, bx                                ; 31 db
    sal ebx, 010h                             ; 66 c1 e3 10
    mov si, ax                                ; 89 c6
    sal esi, 010h                             ; 66 c1 e6 10
    jmp near 08ffah                           ; e9 74 00
    sti                                       ; fb
    hlt                                       ; f4
    jmp near 08ffah                           ; e9 6f 00
    cmp word [bp+010h], strict byte 00003h    ; 83 7e 10 03
    je short 08fb0h                           ; 74 1f
    cmp word [bp+010h], strict byte 00002h    ; 83 7e 10 02
    je short 08fa8h                           ; 74 11
    cmp word [bp+010h], strict byte 00001h    ; 83 7e 10 01
    jne short 08fb8h                          ; 75 1b
    mov dx, 08900h                            ; ba 00 89
    mov ax, 00cfah                            ; b8 fa 0c
    call 08ed2h                               ; e8 2c ff
    jmp short 08ffah                          ; eb 52
    mov dx, 08900h                            ; ba 00 89
    mov ax, 00d02h                            ; b8 02 0d
    jmp short 08fa3h                          ; eb f3
    mov dx, 08900h                            ; ba 00 89
    mov ax, 00d0ah                            ; b8 0a 0d
    jmp short 08fa3h                          ; eb eb
    or ah, 00ah                               ; 80 cc 0a
    mov word [bp+012h], ax                    ; 89 46 12
    mov word [bp+018h], dx                    ; 89 56 18
    jmp short 08ffah                          ; eb 37
    mov word [bp+012h], 00102h                ; c7 46 12 02 01
    jmp short 08ffah                          ; eb 30
    or ah, 080h                               ; 80 cc 80
    jmp short 08fbbh                          ; eb ec
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 57 89
    push word [bp+00ch]                       ; ff 76 0c
    push word [bp+012h]                       ; ff 76 12
    push 00d13h                               ; 68 13 0d
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 8a 89
    add sp, strict byte 00008h                ; 83 c4 08
    mov ax, word [bp+012h]                    ; 8b 46 12
    xor ah, ah                                ; 30 e4
    or ah, 00ch                               ; 80 cc 0c
    mov word [bp+012h], ax                    ; 89 46 12
    or byte [bp+018h], 001h                   ; 80 4e 18 01
    lea sp, [bp-002h]                         ; 8d 66 fe
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn                                      ; c3
pci16_select_reg_:                           ; 0xf9000 LB 0x24
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    and dl, 0fch                              ; 80 e2 fc
    mov bx, dx                                ; 89 d3
    mov dx, 00cf8h                            ; ba f8 0c
    movzx eax, ax                             ; 66 0f b7 c0
    sal eax, 008h                             ; 66 c1 e0 08
    or eax, strict dword 080000000h           ; 66 0d 00 00 00 80
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3
    out DX, eax                               ; 66 ef
    lea sp, [bp-002h]                         ; 8d 66 fe
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
pci16_find_device_:                          ; 0xf9024 LB 0xf7
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    push di                                   ; 57
    sub sp, strict byte 0000ch                ; 83 ec 0c
    push ax                                   ; 50
    push dx                                   ; 52
    mov si, bx                                ; 89 de
    mov di, cx                                ; 89 cf
    test cx, cx                               ; 85 c9
    xor bx, bx                                ; 31 db
    mov byte [bp-008h], 000h                  ; c6 46 f8 00
    test bl, 007h                             ; f6 c3 07
    jne short 0906ch                          ; 75 2d
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, bx                                ; 89 d8
    call 09000h                               ; e8 b9 ff
    mov dx, 00cfeh                            ; ba fe 0c
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov byte [bp-006h], al                    ; 88 46 fa
    cmp AL, strict byte 0ffh                  ; 3c ff
    jne short 0905ah                          ; 75 06
    add bx, strict byte 00008h                ; 83 c3 08
    jmp near 090edh                           ; e9 93 00
    test byte [bp-006h], 080h                 ; f6 46 fa 80
    je short 09067h                           ; 74 07
    mov word [bp-00ah], strict word 00001h    ; c7 46 f6 01 00
    jmp short 0906ch                          ; eb 05
    mov word [bp-00ah], strict word 00008h    ; c7 46 f6 08 00
    mov al, byte [bp-006h]                    ; 8a 46 fa
    and AL, strict byte 007h                  ; 24 07
    cmp AL, strict byte 001h                  ; 3c 01
    jne short 09094h                          ; 75 1f
    mov ax, bx                                ; 89 d8
    shr ax, 008h                              ; c1 e8 08
    test ax, ax                               ; 85 c0
    jne short 09094h                          ; 75 16
    mov dx, strict word 0001ah                ; ba 1a 00
    mov ax, bx                                ; 89 d8
    call 09000h                               ; e8 7a ff
    mov dx, 00cfeh                            ; ba fe 0c
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp al, byte [bp-008h]                    ; 3a 46 f8
    jbe short 09094h                          ; 76 03
    mov byte [bp-008h], al                    ; 88 46 f8
    test di, di                               ; 85 ff
    je short 0909dh                           ; 74 05
    mov dx, strict word 00008h                ; ba 08 00
    jmp short 0909fh                          ; eb 02
    xor dx, dx                                ; 31 d2
    mov ax, bx                                ; 89 d8
    call 09000h                               ; e8 5c ff
    mov dx, 00cfch                            ; ba fc 0c
    in eax, DX                                ; 66 ed
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0
    shr eax, 010h                             ; 66 c1 e8 10
    xchg dx, ax                               ; 92
    mov word [bp-00eh], ax                    ; 89 46 f2
    mov word [bp-00ch], dx                    ; 89 56 f4
    mov word [bp-010h], strict word 00000h    ; c7 46 f0 00 00
    test di, di                               ; 85 ff
    je short 090ceh                           ; 74 0f
    mov cx, strict word 00008h                ; b9 08 00
    shr dx, 1                                 ; d1 ea
    rcr ax, 1                                 ; d1 d8
    loop 090c2h                               ; e2 fa
    mov word [bp-00eh], ax                    ; 89 46 f2
    mov word [bp-00ch], dx                    ; 89 56 f4
    mov ax, word [bp-00ch]                    ; 8b 46 f4
    cmp ax, word [bp-014h]                    ; 3b 46 ec
    jne short 090deh                          ; 75 08
    mov ax, word [bp-00eh]                    ; 8b 46 f2
    cmp ax, word [bp-012h]                    ; 3b 46 ee
    je short 090e4h                           ; 74 06
    cmp word [bp-010h], strict byte 00000h    ; 83 7e f0 00
    je short 090eah                           ; 74 06
    dec si                                    ; 4e
    cmp si, strict byte 0ffffh                ; 83 fe ff
    je short 090fch                           ; 74 12
    add bx, word [bp-00ah]                    ; 03 5e f6
    mov dx, bx                                ; 89 da
    shr dx, 008h                              ; c1 ea 08
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    cmp dx, ax                                ; 39 c2
    jbe near 0903ah                           ; 0f 86 3e ff
    cmp si, strict byte 0ffffh                ; 83 fe ff
    jne short 09105h                          ; 75 04
    mov ax, bx                                ; 89 d8
    jmp short 09108h                          ; eb 03
    mov ax, strict word 0ffffh                ; b8 ff ff
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn                                      ; c3
    std                                       ; fd
    xchg cx, ax                               ; 91
    pop SS                                    ; 17
    xchg dx, ax                               ; 92
    sub dl, byte [bp+si-06dc1h]               ; 2a 92 3f 92
    push dx                                   ; 52
    xchg dx, ax                               ; 92
    db  065h, 092h
    ; gs xchg dx, ax                            ; 65 92
_pci16_function:                             ; 0xf911b LB 0x1d7
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    push di                                   ; 57
    push ax                                   ; 50
    push ax                                   ; 50
    and word [bp+020h], 000ffh                ; 81 66 20 ff 00
    and word [bp+02ch], strict byte 0fffeh    ; 83 66 2c fe
    mov bx, word [bp+020h]                    ; 8b 5e 20
    xor bh, bh                                ; 30 ff
    mov ax, word [bp+020h]                    ; 8b 46 20
    xor ah, ah                                ; 30 e4
    cmp bx, strict byte 00003h                ; 83 fb 03
    jc short 09154h                           ; 72 1a
    jbe short 091ach                          ; 76 70
    cmp bx, strict byte 0000eh                ; 83 fb 0e
    je near 09279h                            ; 0f 84 36 01
    cmp bx, strict byte 00008h                ; 83 fb 08
    jc near 092beh                            ; 0f 82 74 01
    cmp bx, strict byte 0000dh                ; 83 fb 0d
    jbe near 091d1h                           ; 0f 86 80 00
    jmp near 092beh                           ; e9 6a 01
    cmp bx, strict byte 00002h                ; 83 fb 02
    je short 0917ch                           ; 74 23
    cmp bx, strict byte 00001h                ; 83 fb 01
    jne near 092beh                           ; 0f 85 5e 01
    mov word [bp+020h], strict word 00001h    ; c7 46 20 01 00
    mov word [bp+014h], 00210h                ; c7 46 14 10 02
    mov word [bp+01ch], strict word 00000h    ; c7 46 1c 00 00
    mov word [bp+018h], 04350h                ; c7 46 18 50 43
    mov word [bp+01ah], 02049h                ; c7 46 1a 49 20
    jmp near 092ebh                           ; e9 6f 01
    cmp word [bp+018h], strict byte 0ffffh    ; 83 7e 18 ff
    jne short 09188h                          ; 75 06
    or ah, 083h                               ; 80 cc 83
    jmp near 092e4h                           ; e9 5c 01
    mov bx, word [bp+008h]                    ; 8b 5e 08
    mov dx, word [bp+01ch]                    ; 8b 56 1c
    mov ax, word [bp+018h]                    ; 8b 46 18
    xor cx, cx                                ; 31 c9
    call 09024h                               ; e8 8e fe
    cmp ax, strict word 0ffffh                ; 3d ff ff
    jne short 091a6h                          ; 75 0b
    mov ax, word [bp+020h]                    ; 8b 46 20
    xor ah, ah                                ; 30 e4
    or ah, 086h                               ; 80 cc 86
    jmp near 092e4h                           ; e9 3e 01
    mov word [bp+014h], ax                    ; 89 46 14
    jmp near 092ebh                           ; e9 3f 01
    mov bx, word [bp+008h]                    ; 8b 5e 08
    mov ax, word [bp+01ch]                    ; 8b 46 1c
    mov dx, word [bp+01eh]                    ; 8b 56 1e
    mov cx, strict word 00001h                ; b9 01 00
    call 09024h                               ; e8 69 fe
    cmp ax, strict word 0ffffh                ; 3d ff ff
    jne short 091cbh                          ; 75 0b
    mov ax, word [bp+020h]                    ; 8b 46 20
    xor ah, ah                                ; 30 e4
    or ah, 086h                               ; 80 cc 86
    jmp near 092e4h                           ; e9 19 01
    mov word [bp+014h], ax                    ; 89 46 14
    jmp near 092ebh                           ; e9 1a 01
    cmp word [bp+004h], 00100h                ; 81 7e 04 00 01
    jc short 091deh                           ; 72 06
    or ah, 087h                               ; 80 cc 87
    jmp near 092e4h                           ; e9 06 01
    mov dx, word [bp+004h]                    ; 8b 56 04
    mov ax, word [bp+014h]                    ; 8b 46 14
    call 09000h                               ; e8 19 fe
    mov bx, word [bp+020h]                    ; 8b 5e 20
    xor bh, bh                                ; 30 ff
    sub bx, strict byte 00008h                ; 83 eb 08
    cmp bx, strict byte 00005h                ; 83 fb 05
    jnbe near 092ebh                          ; 0f 87 f5 00
    add bx, bx                                ; 01 db
    jmp word [cs:bx-06ef1h]                   ; 2e ff a7 0f 91
    mov bx, word [bp+01ch]                    ; 8b 5e 1c
    xor bl, bl                                ; 30 db
    mov dx, word [bp+004h]                    ; 8b 56 04
    and dx, strict byte 00003h                ; 83 e2 03
    add dx, 00cfch                            ; 81 c2 fc 0c
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    or bx, ax                                 ; 09 c3
    mov word [bp+01ch], bx                    ; 89 5e 1c
    jmp near 092ebh                           ; e9 d4 00
    mov dx, word [bp+004h]                    ; 8b 56 04
    xor dh, dh                                ; 30 f6
    and dl, 002h                              ; 80 e2 02
    add dx, 00cfch                            ; 81 c2 fc 0c
    in ax, DX                                 ; ed
    mov word [bp+01ch], ax                    ; 89 46 1c
    jmp near 092ebh                           ; e9 c1 00
    mov dx, 00cfch                            ; ba fc 0c
    in eax, DX                                ; 66 ed
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0
    shr eax, 010h                             ; 66 c1 e8 10
    xchg dx, ax                               ; 92
    mov word [bp+01ch], ax                    ; 89 46 1c
    mov word [bp+01eh], dx                    ; 89 56 1e
    jmp near 092ebh                           ; e9 ac 00
    mov ax, word [bp+01ch]                    ; 8b 46 1c
    mov dx, word [bp+004h]                    ; 8b 56 04
    xor dh, dh                                ; 30 f6
    and dl, 003h                              ; 80 e2 03
    add dx, 00cfch                            ; 81 c2 fc 0c
    out DX, AL                                ; ee
    jmp near 092ebh                           ; e9 99 00
    mov ax, word [bp+01ch]                    ; 8b 46 1c
    mov dx, word [bp+004h]                    ; 8b 56 04
    xor dh, dh                                ; 30 f6
    and dl, 002h                              ; 80 e2 02
    add dx, 00cfch                            ; 81 c2 fc 0c
    out DX, ax                                ; ef
    jmp near 092ebh                           ; e9 86 00
    mov ax, word [bp+01ch]                    ; 8b 46 1c
    mov cx, word [bp+01eh]                    ; 8b 4e 1e
    mov dx, 00cfch                            ; ba fc 0c
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    jmp short 092ebh                          ; eb 72
    mov bx, word [bp+004h]                    ; 8b 5e 04
    mov es, [bp+026h]                         ; 8e 46 26
    mov word [bp-008h], bx                    ; 89 5e f8
    mov [bp-006h], es                         ; 8c 46 fa
    mov cx, word [0f4a0h]                     ; 8b 0e a0 f4
    cmp cx, word [es:bx]                      ; 26 3b 0f
    jbe short 0929fh                          ; 76 11
    mov ax, word [bp+020h]                    ; 8b 46 20
    xor ah, ah                                ; 30 e4
    or ah, 089h                               ; 80 cc 89
    mov word [bp+020h], ax                    ; 89 46 20
    or word [bp+02ch], strict byte 00001h     ; 83 4e 2c 01
    jmp short 092b3h                          ; eb 14
    les di, [es:bx+002h]                      ; 26 c4 7f 02
    mov si, 0f2c0h                            ; be c0 f2
    mov dx, ds                                ; 8c da
    push DS                                   ; 1e
    mov ds, dx                                ; 8e da
    rep movsb                                 ; f3 a4
    pop DS                                    ; 1f
    mov word [bp+014h], 00a00h                ; c7 46 14 00 0a
    mov ax, word [0f4a0h]                     ; a1 a0 f4
    les bx, [bp-008h]                         ; c4 5e f8
    mov word [es:bx], ax                      ; 26 89 07
    jmp short 092ebh                          ; eb 2d
    mov bx, 00d8ah                            ; bb 8a 0d
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 01931h                               ; e8 68 86
    mov ax, word [bp+014h]                    ; 8b 46 14
    push ax                                   ; 50
    mov ax, word [bp+020h]                    ; 8b 46 20
    push ax                                   ; 50
    push 00d46h                               ; 68 46 0d
    push strict byte 00004h                   ; 6a 04
    call 01972h                               ; e8 99 86
    add sp, strict byte 00008h                ; 83 c4 08
    mov ax, word [bp+020h]                    ; 8b 46 20
    xor ah, ah                                ; 30 e4
    or ah, 081h                               ; 80 cc 81
    mov word [bp+020h], ax                    ; 89 46 20
    or word [bp+02ch], strict byte 00001h     ; 83 4e 2c 01
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn                                      ; c3
pci_find_classcode_:                         ; 0xf92f2 LB 0x2b
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    push cx                                   ; 51
    push si                                   ; 56
    mov cx, dx                                ; 89 d1
    xor si, si                                ; 31 f6
    mov dx, ax                                ; 89 c2
    mov ax, 0b103h                            ; b8 03 b1
    sal ecx, 010h                             ; 66 c1 e1 10
    db  08bh, 0cah
    ; mov cx, dx                                ; 8b ca
    int 01ah                                  ; cd 1a
    cmp ah, 000h                              ; 80 fc 00
    je near 09313h                            ; 0f 84 03 00
    mov bx, strict word 0ffffh                ; bb ff ff
    mov ax, bx                                ; 89 d8
    lea sp, [bp-006h]                         ; 8d 66 fa
    pop si                                    ; 5e
    pop cx                                    ; 59
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
pci_read_config_byte_:                       ; 0xf931d LB 0x24
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push cx                                   ; 51
    push di                                   ; 57
    movzx di, bl                              ; 0f b6 fb
    movzx bx, al                              ; 0f b6 d8
    sal bx, 008h                              ; c1 e3 08
    movzx ax, dl                              ; 0f b6 c2
    or bx, ax                                 ; 09 c3
    mov ax, 0b108h                            ; b8 08 b1
    int 01ah                                  ; cd 1a
    movzx ax, cl                              ; 0f b6 c1
    xor dx, dx                                ; 31 d2
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop di                                    ; 5f
    pop cx                                    ; 59
    pop bp                                    ; 5d
    retn                                      ; c3
pci_read_config_word_:                       ; 0xf9341 LB 0x22
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push cx                                   ; 51
    push di                                   ; 57
    movzx di, bl                              ; 0f b6 fb
    xor ah, ah                                ; 30 e4
    sal ax, 008h                              ; c1 e0 08
    movzx bx, dl                              ; 0f b6 da
    or bx, ax                                 ; 09 c3
    mov ax, 0b109h                            ; b8 09 b1
    int 01ah                                  ; cd 1a
    mov ax, cx                                ; 89 c8
    xor dx, dx                                ; 31 d2
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop di                                    ; 5f
    pop cx                                    ; 59
    pop bp                                    ; 5d
    retn                                      ; c3
pci_read_config_dword_:                      ; 0xf9363 LB 0x27
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push cx                                   ; 51
    push di                                   ; 57
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
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop di                                    ; 5f
    pop cx                                    ; 59
    pop bp                                    ; 5d
    retn                                      ; c3
vds_is_present_:                             ; 0xf938a LB 0x1d
    push bx                                   ; 53
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, strict word 0007bh                ; bb 7b 00
    mov ax, strict word 00040h                ; b8 40 00
    mov es, ax                                ; 8e c0
    test byte [es:bx], 020h                   ; 26 f6 07 20
    je short 093a2h                           ; 74 06
    mov ax, strict word 00001h                ; b8 01 00
    pop bp                                    ; 5d
    pop bx                                    ; 5b
    retn                                      ; c3
    xor ax, ax                                ; 31 c0
    pop bp                                    ; 5d
    pop bx                                    ; 5b
    retn                                      ; c3
vds_real_to_lin_:                            ; 0xf93a7 LB 0x1e
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
    loop 093b5h                               ; e2 fa
    xor cx, cx                                ; 31 c9
    add ax, bx                                ; 01 d8
    adc dx, cx                                ; 11 ca
    pop bp                                    ; 5d
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
vds_build_sg_list_:                          ; 0xf93c5 LB 0x79
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push si                                   ; 56
    push di                                   ; 57
    mov di, ax                                ; 89 c7
    mov si, dx                                ; 89 d6
    mov ax, bx                                ; 89 d8
    mov dx, cx                                ; 89 ca
    mov bx, word [bp+004h]                    ; 8b 5e 04
    mov es, si                                ; 8e c6
    mov word [es:di], bx                      ; 26 89 1d
    mov bx, word [bp+006h]                    ; 8b 5e 06
    mov word [es:di+002h], bx                 ; 26 89 5d 02
    call 093a7h                               ; e8 c3 ff
    mov es, si                                ; 8e c6
    mov word [es:di+004h], ax                 ; 26 89 45 04
    mov word [es:di+006h], dx                 ; 26 89 55 06
    mov word [es:di+008h], strict word 00000h ; 26 c7 45 08 00 00
    call 0938ah                               ; e8 93 ff
    test ax, ax                               ; 85 c0
    je short 0940eh                           ; 74 13
    mov es, si                                ; 8e c6
    mov ax, 08105h                            ; b8 05 81
    mov dx, strict word 00000h                ; ba 00 00
    int 04bh                                  ; cd 4b
    jc near 0940bh                            ; 0f 82 02 00
    db  032h, 0c0h
    ; xor al, al                                ; 32 c0
    cbw                                       ; 98
    jmp short 09435h                          ; eb 27
    mov es, si                                ; 8e c6
    mov word [es:di+00eh], strict word 00001h ; 26 c7 45 0e 01 00
    mov dx, word [es:di+004h]                 ; 26 8b 55 04
    mov ax, word [es:di+006h]                 ; 26 8b 45 06
    mov word [es:di+010h], dx                 ; 26 89 55 10
    mov word [es:di+012h], ax                 ; 26 89 45 12
    mov ax, word [bp+004h]                    ; 8b 46 04
    mov word [es:di+014h], ax                 ; 26 89 45 14
    mov ax, bx                                ; 89 d8
    mov word [es:di+016h], bx                 ; 26 89 5d 16
    xor ax, bx                                ; 31 d8
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn 00004h                               ; c2 04 00
vds_free_sg_list_:                           ; 0xf943e LB 0x32
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    push di                                   ; 57
    mov bx, ax                                ; 89 c3
    call 0938ah                               ; e8 42 ff
    test ax, ax                               ; 85 c0
    je short 0945fh                           ; 74 13
    mov di, bx                                ; 89 df
    mov es, dx                                ; 8e c2
    mov ax, 08106h                            ; b8 06 81
    mov dx, strict word 00000h                ; ba 00 00
    int 04bh                                  ; cd 4b
    jc near 0945eh                            ; 0f 82 02 00
    db  032h, 0c0h
    ; xor al, al                                ; 32 c0
    cbw                                       ; 98
    mov es, dx                                ; 8e c2
    mov word [es:bx+00eh], strict word 00000h ; 26 c7 47 0e 00 00
    lea sp, [bp-004h]                         ; 8d 66 fc
    pop di                                    ; 5f
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
    times 0x2 db 0
__U4D:                                       ; 0xf9470 LB 0x39
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
__U4M:                                       ; 0xf94a9 LB 0x31
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
_fmemset_:                                   ; 0xf94da LB 0xd
    push di                                   ; 57
    mov es, dx                                ; 8e c2
    db  08bh, 0f8h
    ; mov di, ax                                ; 8b f8
    xchg al, bl                               ; 86 d8
    rep stosb                                 ; f3 aa
    xchg al, bl                               ; 86 d8
    pop di                                    ; 5f
    retn                                      ; c3
_fmemcpy_:                                   ; 0xf94e7 LB 0x33
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
    add byte [bx+si], dh                      ; 00 30
    xchg bp, ax                               ; 95
    xor dl, byte [di-06acah]                  ; 32 95 36 95
    db  036h, 095h
    ; ss xchg bp, ax                            ; 36 95
    db  036h, 095h
    ; ss xchg bp, ax                            ; 36 95
    cmp byte [di-06ac8h], dl                  ; 38 95 38 95
    cmp dl, byte [di-06ac2h]                  ; 3a 95 3e 95
    db  03eh, 095h
    ; ds xchg bp, ax                            ; 3e 95
    inc ax                                    ; 40
    xchg bp, ax                               ; 95
    inc bp                                    ; 45
    xchg bp, ax                               ; 95
    inc di                                    ; 47
    xchg bp, ax                               ; 95
apm_worker:                                  ; 0xf951a LB 0x3a
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
    jnc short 09550h                          ; 73 25
    jmp word [cs:bp-06b00h]                   ; 2e ff a6 00 95
    jmp short 0954eh                          ; eb 1c
    sti                                       ; fb
    hlt                                       ; f4
    jmp short 0954eh                          ; eb 18
    jmp short 0954eh                          ; eb 16
    jmp short 09550h                          ; eb 16
    mov AH, strict byte 080h                  ; b4 80
    jmp short 09552h                          ; eb 14
    jmp short 09550h                          ; eb 10
    mov ax, 00102h                            ; b8 02 01
    jmp short 0954eh                          ; eb 09
    jmp short 0954eh                          ; eb 07
    mov BL, strict byte 000h                  ; b3 00
    mov cx, strict word 00000h                ; b9 00 00
    jmp short 0954eh                          ; eb 00
    clc                                       ; f8
    retn                                      ; c3
    mov AH, strict byte 009h                  ; b4 09
    stc                                       ; f9
    retn                                      ; c3
apm_pm16_entry:                              ; 0xf9554 LB 0x11
    mov AH, strict byte 002h                  ; b4 02
    push DS                                   ; 1e
    push bp                                   ; 55
    push CS                                   ; 0e
    pop bp                                    ; 5d
    add bp, strict byte 00008h                ; 83 c5 08
    mov ds, bp                                ; 8e dd
    call 0951ah                               ; e8 b8 ff
    pop bp                                    ; 5d
    pop DS                                    ; 1f
    retf                                      ; cb

  ; Padding 0x449b bytes at 0xf9565
  times 17563 db 0

section BIOS32 progbits vstart=0xda00 align=1 ; size=0x3cb class=CODE group=AUTO
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
    call 0db78h                               ; e8 4b 01
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
    mov bp, 09556h                            ; bd 56 95
    add byte [bx+si], al                      ; 00 00
    push ebp                                  ; 66 55
    mov AH, strict byte 003h                  ; b4 03
    db  066h, 0cbh
    ; retf                                      ; 66 cb
    pop bp                                    ; 5d
    retf                                      ; cb
pci32_select_reg_:                           ; 0xfda61 LB 0x22
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
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
    lea sp, [di-004h]                         ; 8d 65 fc
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3
pci32_find_device_:                          ; 0xfda83 LB 0xf7
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    sub sp, strict byte 00014h                ; 83 ec 14
    push ax                                   ; 50
    mov cx, dx                                ; 89 d1
    mov si, bx                                ; 89 de
    test bx, bx                               ; 85 db
    xor bx, bx                                ; 31 db
    mov byte [di-010h], 000h                  ; c6 45 f0 00
    test bl, 007h                             ; f6 c3 07
    jne short 0dad4h                          ; 75 36
    db  00fh, 0b7h, 0c3h
    ; movzx ax, bx                              ; 0f b7 c3
    mov dx, strict word 0000eh                ; ba 0e 00
    add byte [bx+si], al                      ; 00 00
    call 0da5fh                               ; e8 b6 ff
    db  0ffh
    db  0ffh
    mov dx, 00cfeh                            ; ba fe 0c
    add byte [bx+si], al                      ; 00 00
    db  02bh, 0c0h
    ; sub ax, ax                                ; 2b c0
    in AL, DX                                 ; ec
    mov byte [di-014h], al                    ; 88 45 ec
    cmp AL, strict byte 0ffh                  ; 3c ff
    jne short 0dac2h                          ; 75 08
    add bx, strict byte 00008h                ; 83 c3 08
    jmp near 0db4ah                           ; e9 8a 00
    add byte [bx+si], al                      ; 00 00
    test byte [di-014h], 080h                 ; f6 45 ec 80
    je short 0dacfh                           ; 74 07
    mov di, strict word 00001h                ; bf 01 00
    add byte [bx+si], al                      ; 00 00
    jmp short 0dad4h                          ; eb 05
    mov di, strict word 00008h                ; bf 08 00
    add byte [bx+si], al                      ; 00 00
    mov al, byte [di-014h]                    ; 8a 45 ec
    and AL, strict byte 007h                  ; 24 07
    cmp AL, strict byte 001h                  ; 3c 01
    jne short 0db03h                          ; 75 26
    db  00fh, 0b7h, 0c3h
    ; movzx ax, bx                              ; 0f b7 c3
    mov dx, ax                                ; 89 c2
    sar dx, 008h                              ; c1 fa 08
    test dx, dx                               ; 85 d2
    jne short 0db03h                          ; 75 1a
    mov dx, strict word 0001ah                ; ba 1a 00
    add byte [bx+si], al                      ; 00 00
    call 0da5fh                               ; e8 6e ff
    db  0ffh
    db  0ffh
    mov dx, 00cfeh                            ; ba fe 0c
    add byte [bx+si], al                      ; 00 00
    db  02bh, 0c0h
    ; sub ax, ax                                ; 2b c0
    in AL, DX                                 ; ec
    cmp al, byte [di-010h]                    ; 3a 45 f0
    jbe short 0db03h                          ; 76 03
    mov byte [di-010h], al                    ; 88 45 f0
    test si, si                               ; 85 f6
    je short 0db0eh                           ; 74 07
    mov ax, strict word 00008h                ; b8 08 00
    add byte [bx+si], al                      ; 00 00
    jmp short 0db10h                          ; eb 02
    xor ax, ax                                ; 31 c0
    db  00fh, 0b7h, 0d0h
    ; movzx dx, ax                              ; 0f b7 d0
    db  00fh, 0b7h, 0c3h
    ; movzx ax, bx                              ; 0f b7 c3
    call 0da5fh                               ; e8 46 ff
    db  0ffh
    db  0ffh
    mov dx, 00cfch                            ; ba fc 0c
    add byte [bx+si], al                      ; 00 00
    in ax, DX                                 ; ed
    mov word [di-018h], ax                    ; 89 45 e8
    mov word [di-020h], strict word 00000h    ; c7 45 e0 00 00
    add byte [bx+si], al                      ; 00 00
    test si, si                               ; 85 f6
    je short 0db35h                           ; 74 06
    shr ax, 008h                              ; c1 e8 08
    mov word [di-018h], ax                    ; 89 45 e8
    mov ax, word [di-018h]                    ; 8b 45 e8
    cmp ax, word [di-024h]                    ; 3b 45 dc
    je short 0db43h                           ; 74 06
    cmp word [di-020h], strict byte 00000h    ; 83 7d e0 00
    je short 0db4ah                           ; 74 07
    dec cx                                    ; 49
    cmp ecx, strict byte 0ffffffffh           ; 66 83 f9 ff
    je short 0db62h                           ; 74 18
    add bx, di                                ; 01 fb
    db  00fh, 0b7h, 0c3h
    ; movzx ax, bx                              ; 0f b7 c3
    sar ax, 008h                              ; c1 f8 08
    mov word [di-01ch], ax                    ; 89 45 e4
    movzx ax, byte [di-010h]                  ; 0f b6 45 f0
    cmp ax, word [di-01ch]                    ; 3b 45 e4
    jnl near 0da97h                           ; 0f 8d 37 ff
    db  0ffh
    jmp word [bp-07dh]                        ; ff 66 83
    stc                                       ; f9
    push word [di+005h]                       ; ff 75 05
    db  00fh, 0b7h, 0c3h
    ; movzx ax, bx                              ; 0f b7 c3
    jmp short 0db72h                          ; eb 05
    mov ax, strict word 0ffffh                ; b8 ff ff
    add byte [bx+si], al                      ; 00 00
    lea sp, [di-00ch]                         ; 8d 65 f4
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    pop bp                                    ; 5d
    retn                                      ; c3
_pci32_function:                             ; 0xfdb7a LB 0x251
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push bx                                   ; 53
    push si                                   ; 56
    push di                                   ; 57
    push ax                                   ; 50
    push ax                                   ; 50
    and dword [di+024h], strict dword 0658100ffh ; 66 81 65 24 ff 00 81 65
    sub AL, strict byte 0feh                  ; 2c fe
    inc word [bx+si]                          ; ff 00
    add byte [bp+di+02445h], cl               ; 00 8b 45 24
    xor ah, ah                                ; 30 e4
    cmp eax, strict dword 029720003h          ; 66 3d 03 00 72 29
    jbe near 0dc37h                           ; 0f 86 99 00
    add byte [bx+si], al                      ; 00 00
    cmp eax, strict dword 0840f000eh          ; 66 3d 0e 00 0f 84
    test ax, strict word 00001h               ; a9 01 00
    add byte [bp+03dh], ah                    ; 00 66 3d
    or byte [bx+si], al                       ; 08 00
    jc near 0ddb1h                            ; 0f 82 ff 01
    add byte [bx+si], al                      ; 00 00
    cmp eax, strict dword 0860f000dh          ; 66 3d 0d 00 0f 86
    test AL, strict byte 000h                 ; a8 00
    add byte [bx+si], al                      ; 00 00
    jmp near 0ddb1h                           ; e9 f0 01
    add byte [bx+si], al                      ; 00 00
    cmp eax, strict dword 028740002h          ; 66 3d 02 00 74 28
    cmp eax, strict dword 0850f0001h          ; 66 3d 01 00 0f 85
    loopne 0dbd2h                             ; e0 01
    add byte [bx+si], al                      ; 00 00
    mov dword [di+024h], strict dword 0c7660001h ; 66 c7 45 24 01 00 66 c7
    inc bp                                    ; 45
    sbb byte [bx+si], dl                      ; 18 10
    add dh, byte [bx+di]                      ; 02 31
    sal byte [bp-077h], 045h                  ; c0 66 89 45
    and bh, al                                ; 20 c7
    inc bp                                    ; 45
    sbb AL, strict byte 050h                  ; 1c 50
    inc bx                                    ; 43
    dec cx                                    ; 49
    and cl, ch                                ; 20 e9
    rol byte [bx+di], CL                      ; d2 01
    add byte [bx+si], al                      ; 00 00
    cmp dword [di+01ch], strict byte 0ffffffffh ; 66 83 7d 1c ff
    jne short 0dc05h                          ; 75 0d
    mov ax, word [di+024h]                    ; 8b 45 24
    xor ah, ah                                ; 30 e4
    or ah, 083h                               ; 80 cc 83
    jmp near 0ddb9h                           ; e9 b6 01
    add byte [bx+si], al                      ; 00 00
    xor bx, bx                                ; 31 db
    db  00fh, 0b7h, 055h, 00ch
    ; movzx dx, word [di+00ch]                  ; 0f b7 55 0c
    db  00fh, 0b7h, 045h, 020h
    ; movzx ax, word [di+020h]                  ; 0f b7 45 20
    sal ax, 010h                              ; c1 e0 10
    db  00fh, 0b7h, 04dh, 01ch
    ; movzx cx, word [di+01ch]                  ; 0f b7 4d 1c
    or ax, cx                                 ; 09 c8
    call 0da81h                               ; e8 66 fe
    db  0ffh
    jmp word [bp+03dh]                        ; ff 66 3d
    db  0ffh
    push word [di+00dh]                       ; ff 75 0d
    mov ax, word [di+024h]                    ; 8b 45 24
    xor ah, ah                                ; 30 e4
    or ah, 086h                               ; 80 cc 86
    jmp near 0ddb9h                           ; e9 8b 01
    add byte [bx+si], al                      ; 00 00
    mov dword [di+018h], eax                  ; 66 89 45 18
    jmp near 0ddc1h                           ; e9 8a 01
    add byte [bx+si], al                      ; 00 00
    db  00fh, 0b7h, 055h, 00ch
    ; movzx dx, word [di+00ch]                  ; 0f b7 55 0c
    mov ax, word [di+020h]                    ; 8b 45 20
    mov bx, strict word 00001h                ; bb 01 00
    add byte [bx+si], al                      ; 00 00
    call 0da81h                               ; e8 39 fe
    db  0ffh
    jmp word [bp+03dh]                        ; ff 66 3d
    db  0ffh
    push word [di+00dh]                       ; ff 75 0d
    mov ax, word [di+024h]                    ; 8b 45 24
    xor ah, ah                                ; 30 e4
    or ah, 086h                               ; 80 cc 86
    jmp near 0ddb9h                           ; e9 5e 01
    add byte [bx+si], al                      ; 00 00
    mov dword [di+018h], eax                  ; 66 89 45 18
    jmp near 0ddc1h                           ; e9 5d 01
    add byte [bx+si], al                      ; 00 00
    cmp dword [di+008h], strict dword 00d720100h ; 66 81 7d 08 00 01 72 0d
    mov ax, word [di+024h]                    ; 8b 45 24
    xor ah, ah                                ; 30 e4
    or ah, 087h                               ; 80 cc 87
    jmp near 0ddb9h                           ; e9 40 01
    add byte [bx+si], al                      ; 00 00
    db  00fh, 0b7h, 055h, 008h
    ; movzx dx, word [di+008h]                  ; 0f b7 55 08
    db  00fh, 0b7h, 045h, 018h
    ; movzx ax, word [di+018h]                  ; 0f b7 45 18
    call 0da5fh                               ; e8 d9 fd
    db  0ffh
    dec word [bp+di+02445h]                   ; ff 8b 45 24
    xor ah, ah                                ; 30 e4
    cmp eax, strict dword 02172000ah          ; 66 3d 0a 00 72 21
    jbe short 0dd04h                          ; 76 6f
    cmp eax, strict dword 0840f000dh          ; 66 3d 0d 00 0f 84
    test ax, strict word 00000h               ; a9 00 00
    add byte [bp+03dh], ah                    ; 00 66 3d
    or AL, strict byte 000h                   ; 0c 00
    je near 0dd2ah                            ; 0f 84 83 00
    add byte [bx+si], al                      ; 00 00
    cmp eax, strict dword 06374000bh          ; 66 3d 0b 00 74 63
    jmp near 0ddc1h                           ; e9 0f 01
    add byte [bx+si], al                      ; 00 00
    cmp eax, strict dword 02d740009h          ; 66 3d 09 00 74 2d
    cmp eax, strict dword 0850f0008h          ; 66 3d 08 00 0f 85
    inc word [bx+si]                          ; ff 00
    add byte [bx+si], al                      ; 00 00
    mov bx, word [di+020h]                    ; 8b 5d 20
    xor bl, bl                                ; 30 db
    mov ax, word [di+008h]                    ; 8b 45 08
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
    mov dword [di+020h], ebx                  ; 66 89 5d 20
    jmp near 0ddc1h                           ; e9 dc 00
    add byte [bx+si], al                      ; 00 00
    mov ax, word [di+008h]                    ; 8b 45 08
    xor ah, ah                                ; 30 e4
    and AL, strict byte 002h                  ; 24 02
    db  00fh, 0b7h, 0d0h
    ; movzx dx, ax                              ; 0f b7 d0
    add dx, 00cfch                            ; 81 c2 fc 0c
    add byte [bx+si], al                      ; 00 00
    db  02bh, 0c0h
    ; sub ax, ax                                ; 2b c0
    in eax, DX                                ; 66 ed
    mov dword [di+020h], eax                  ; 66 89 45 20
    jmp near 0ddc1h                           ; e9 bf 00
    add byte [bx+si], al                      ; 00 00
    mov dx, 00cfch                            ; ba fc 0c
    add byte [bx+si], al                      ; 00 00
    in ax, DX                                 ; ed
    mov word [di+020h], ax                    ; 89 45 20
    jmp near 0ddc1h                           ; e9 b1 00
    add byte [bx+si], al                      ; 00 00
    mov ax, word [di+020h]                    ; 8b 45 20
    mov dx, word [di+008h]                    ; 8b 55 08
    xor dh, dh                                ; 30 f6
    and dl, 003h                              ; 80 e2 03
    db  00fh, 0b7h, 0d2h
    ; movzx dx, dx                              ; 0f b7 d2
    add dx, 00cfch                            ; 81 c2 fc 0c
    add byte [bx+si], al                      ; 00 00
    out DX, AL                                ; ee
    jmp near 0ddc1h                           ; e9 97 00
    add byte [bx+si], al                      ; 00 00
    db  00fh, 0b7h, 045h, 020h
    ; movzx ax, word [di+020h]                  ; 0f b7 45 20
    mov dx, word [di+008h]                    ; 8b 55 08
    xor dh, dh                                ; 30 f6
    and dl, 002h                              ; 80 e2 02
    db  00fh, 0b7h, 0d2h
    ; movzx dx, dx                              ; 0f b7 d2
    add dx, 00cfch                            ; 81 c2 fc 0c
    add byte [bx+si], al                      ; 00 00
    out DX, eax                               ; 66 ef
    jmp near 0ddc1h                           ; e9 7b 00
    add byte [bx+si], al                      ; 00 00
    mov ax, word [di+020h]                    ; 8b 45 20
    mov dx, 00cfch                            ; ba fc 0c
    add byte [bx+si], al                      ; 00 00
    out DX, ax                                ; ef
    jmp short 0ddc3h                          ; eb 70
    db  00fh, 0b7h, 045h, 008h
    ; movzx ax, word [di+008h]                  ; 0f b7 45 08
    mov es, [di+028h]                         ; 8e 45 28
    mov [di-010h], es                         ; 8c 45 f0
    mov bx, ax                                ; 89 c3
    mov edx, dword [di]                       ; 66 8b 15
    mov AL, byte [000f4h]                     ; a0 f4 00
    add byte [bp+026h], ah                    ; 00 66 26
    cmp dx, word [bx+si]                      ; 3b 10
    jbe short 0dd7eh                          ; 76 12
    mov ax, word [di+024h]                    ; 8b 45 24
    xor ah, ah                                ; 30 e4
    or ah, 089h                               ; 80 cc 89
    mov dword [di+024h], eax                  ; 66 89 45 24
    or word [di+02ch], strict byte 00001h     ; 83 4d 2c 01
    jmp short 0dda4h                          ; eb 26
    db  00fh, 0b7h, 0cah
    ; movzx cx, dx                              ; 0f b7 ca
    db  066h, 026h, 08bh, 050h, 006h
    ; mov edx, dword [es:bx+si+006h]            ; 66 26 8b 50 06
    mov word [di-014h], dx                    ; 89 55 ec
    mov di, word [es:bx+si+002h]              ; 26 8b 78 02
    mov dx, ds                                ; 8c da
    mov si, 0f2c0h                            ; be c0 f2
    add byte [bx+si], al                      ; 00 00
    mov es, [di-014h]                         ; 8e 45 ec
    push DS                                   ; 1e
    db  066h, 08eh, 0dah
    ; mov ds, edx                               ; 66 8e da
    rep movsb                                 ; f3 a4
    pop DS                                    ; 1f
    mov dword [di+018h], strict dword 0a1660a00h ; 66 c7 45 18 00 0a 66 a1
    mov AL, byte [000f4h]                     ; a0 f4 00
    add byte [bp-00fbbh], cl                  ; 00 8e 45 f0
    db  066h, 026h, 089h, 003h
    ; mov dword [es:bp+di], eax                 ; 66 26 89 03
    jmp short 0ddc3h                          ; eb 10
    mov ax, word [di+024h]                    ; 8b 45 24
    xor ah, ah                                ; 30 e4
    or ah, 081h                               ; 80 cc 81
    mov dword [di+024h], eax                  ; 66 89 45 24
    or word [di+02ch], strict byte 00001h     ; 83 4d 2c 01
    lea sp, [di-00ch]                         ; 8d 65 f4
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop bx                                    ; 5b
    pop bp                                    ; 5d
    retn                                      ; c3

  ; Padding 0x1 bytes at 0xfddcb
  times 1 db 0

section BIOS32CONST progbits vstart=0xddcc align=1 ; size=0x0 class=FAR_DATA group=BIOS32_GROUP

section BIOS32CONST2 progbits vstart=0xddcc align=1 ; size=0x0 class=FAR_DATA group=BIOS32_GROUP

section BIOS32_DATA progbits vstart=0xddcc align=1 ; size=0x0 class=FAR_DATA group=BIOS32_GROUP

  ; Padding 0x234 bytes at 0xfddcc
  times 564 db 0

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
post:                                        ; 0xfe05b LB 0x4c
    cli                                       ; fa
    smsw ax                                   ; 0f 01 e0
    test ax, strict word 00001h               ; a9 01 00
    je short 0e068h                           ; 74 04
    out strict byte 092h, AL                  ; e6 92
    jmp short 0e066h                          ; eb fe
    mov AL, strict byte 00fh                  ; b0 0f
    out strict byte 070h, AL                  ; e6 70
    in AL, strict byte 071h                   ; e4 71
    xchg ah, al                               ; 86 c4
    mov AL, strict byte 00fh                  ; b0 0f
    out strict byte 070h, AL                  ; e6 70
    mov AL, strict byte 000h                  ; b0 00
    out strict byte 071h, AL                  ; e6 71
    db  08ah, 0c4h
    ; mov al, ah                                ; 8a c4
    cmp AL, strict byte 009h                  ; 3c 09
    je short 0e090h                           ; 74 12
    cmp AL, strict byte 00ah                  ; 3c 0a
    je short 0e090h                           ; 74 0e
    db  032h, 0c0h
    ; xor al, al                                ; 32 c0
    out strict byte 00dh, AL                  ; e6 0d
    out strict byte 0dah, AL                  ; e6 da
    mov AL, strict byte 0c0h                  ; b0 c0
    out strict byte 0d6h, AL                  ; e6 d6
    mov AL, strict byte 000h                  ; b0 00
    out strict byte 0d4h, AL                  ; e6 d4
    db  08ah, 0c4h
    ; mov al, ah                                ; 8a c4
    cmp AL, strict byte 000h                  ; 3c 00
    je short 0e0a7h                           ; 74 11
    cmp AL, strict byte 00dh                  ; 3c 0d
    jnc short 0e0a7h                          ; 73 0d
    cmp AL, strict byte 009h                  ; 3c 09
    jne short 0e0a1h                          ; 75 03
    jmp near 0e34bh                           ; e9 aa 02
    cmp AL, strict byte 005h                  ; 3c 05
    je short 0e030h                           ; 74 8b
    jmp short 0e0a7h                          ; eb 00
normal_post:                                 ; 0xfe0a7 LB 0x21c
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
    jnc short 0e0dah                          ; 73 0b
    mov es, bx                                ; 8e c3
    db  033h, 0ffh
    ; xor di, di                                ; 33 ff
    mov cx, 08000h                            ; b9 00 80
    rep stosw                                 ; f3 ab
    jmp short 0e0c5h                          ; eb eb
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
    call 01777h                               ; e8 8c 36
    call 0e8e0h                               ; e8 f2 07
    db  033h, 0dbh
    ; xor bx, bx                                ; 33 db
    mov ds, bx                                ; 8e db
    mov cx, strict word 00060h                ; b9 60 00
    mov ax, 0ff53h                            ; b8 53 ff
    mov dx, 0f000h                            ; ba 00 f0
    call 0e044h                               ; e8 46 ff
    mov bx, 001a0h                            ; bb a0 01
    mov cx, strict word 00010h                ; b9 10 00
    call 0e044h                               ; e8 3d ff
    mov ax, 0027fh                            ; b8 7f 02
    mov word [00413h], ax                     ; a3 13 04
    mov ax, 0e9dch                            ; b8 dc e9
    mov word [00018h], ax                     ; a3 18 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [0001ah], ax                     ; a3 1a 00
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
    call 0e778h                               ; e8 08 06
    call 0f13ch                               ; e8 c9 0f
    call 0f1c1h                               ; e8 4b 10
    mov ax, 0fea5h                            ; b8 a5 fe
    mov word [00020h], ax                     ; a3 20 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [00022h], ax                     ; a3 22 00
    mov AL, strict byte 034h                  ; b0 34
    out strict byte 043h, AL                  ; e6 43
    mov AL, strict byte 000h                  ; b0 00
    out strict byte 040h, AL                  ; e6 40
    out strict byte 040h, AL                  ; e6 40
    mov ax, 0f065h                            ; b8 65 f0
    mov word [00040h], ax                     ; a3 40 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [00042h], ax                     ; a3 42 00
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
    mov ax, 0c000h                            ; b8 00 c0
    mov dx, 0c800h                            ; ba 00 c8
    call 01600h                               ; e8 15 34
    call 049b4h                               ; e8 c6 67
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
    call 0ecedh                               ; e8 db 0a
    mov dx, 00278h                            ; ba 78 02
    call 0ecedh                               ; e8 d5 0a
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
    call 0ed0bh                               ; e8 b7 0a
    mov dx, 002f8h                            ; ba f8 02
    call 0ed0bh                               ; e8 b1 0a
    mov dx, 003e8h                            ; ba e8 03
    call 0ed0bh                               ; e8 ab 0a
    mov dx, 002e8h                            ; ba e8 02
    call 0ed0bh                               ; e8 a5 0a
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
    call 0edbfh                               ; e8 24 0b
    mov ax, 0f8a9h                            ; b8 a9 f8
    mov word [001d0h], ax                     ; a3 d0 01
    mov ax, 0f000h                            ; b8 00 f0
    mov word [001d2h], ax                     ; a3 d2 01
    mov ax, 0e2cah                            ; b8 ca e2
    mov word [001d4h], ax                     ; a3 d4 01
    mov ax, 0f000h                            ; b8 00 f0
    mov word [001d6h], ax                     ; a3 d6 01
    call 0e753h                               ; e8 9d 04
    jmp short 0e31bh                          ; eb 63
    times 0x9 db 0
    db  'XM'
nmi:                                         ; 0xfe2c3 LB 0x7
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    call 01753h                               ; e8 8a 34
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
    mov ax, 0f8d7h                            ; b8 d7 f8
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
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    call 01badh                               ; e8 8c 38
    call 01fa7h                               ; e8 83 3c
    call 08dd4h                               ; e8 ad aa
    call 07e7bh                               ; e8 51 9b
    call 0ed2fh                               ; e8 02 0a
    call 0e2d2h                               ; e8 a2 ff
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    call 01797h                               ; e8 61 34
    mov ax, 0c800h                            ; b8 00 c8
    mov dx, 0f000h                            ; ba 00 f0
    call 01600h                               ; e8 c1 32
    call 03698h                               ; e8 56 53
    sti                                       ; fb
    int 019h                                  ; cd 19
    sti                                       ; fb
    hlt                                       ; f4
    jmp short 0e346h                          ; eb fd
    cli                                       ; fa
    hlt                                       ; f4
    mov ax, strict word 00040h                ; b8 40 00
    mov ds, ax                                ; 8e d8
    mov ss, [word 00069h]                     ; 8e 16 69 00
    mov sp, word [word 00067h]                ; 8b 26 67 00
    in AL, strict byte 092h                   ; e4 92
    and AL, strict byte 0fdh                  ; 24 fd
    out strict byte 092h, AL                  ; e6 92
    lidt [cs:0efe7h]                          ; 2e 0f 01 1e e7 ef
    pop DS                                    ; 1f
    pop ES                                    ; 07
    db  08bh, 0ech
    ; mov bp, sp                                ; 8b ec
    in AL, strict byte 080h                   ; e4 80
    mov byte [bp+00fh], al                    ; 88 46 0f
    db  03ah, 0e0h
    ; cmp ah, al                                ; 3a e0
    popaw                                     ; 61
    sti                                       ; fb
    retf 00002h                               ; ca 02 00
    times 0x88 db 0
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
    call 05d34h                               ; e8 f2 75
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
    call 016e8h                               ; e8 99 2f
    popaw                                     ; 61
    pop ES                                    ; 07
    pop DS                                    ; 1f
    iret                                      ; cf
init_pic:                                    ; 0xfe753 LB 0x25
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
ebda_post:                                   ; 0xfe778 LB 0xec
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
    times 0x6f db 0
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
    call 050a2h                               ; e8 60 68
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
    call 050a2h                               ; e8 42 68
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
    call 04cb1h                               ; e8 e2 62
    pop ES                                    ; 07
    popaw                                     ; 61
    pop DS                                    ; 1f
    cli                                       ; fa
    call 0e03fh                               ; e8 69 f6
    mov AL, strict byte 0aeh                  ; b0 ae
    out strict byte 064h, AL                  ; e6 64
    pop ax                                    ; 58
    iret                                      ; cf
    pushaw                                    ; 60
    push ES                                   ; 06
    push DS                                   ; 1e
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    call 0678ch                               ; e8 a7 7d
    pop DS                                    ; 1f
    pop ES                                    ; 07
    popaw                                     ; 61
    iret                                      ; cf
    times 0x26e db 0
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
    jmp near 036dch                           ; e9 6b 4a
    push ES                                   ; 06
    push ax                                   ; 50
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    call 036b0h                               ; e8 37 4a
    cmp AL, strict byte 000h                  ; 3c 00
    je short 0ecabh                           ; 74 2e
    call 036c6h                               ; e8 46 4a
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
    jmp near 03cb0h                           ; e9 19 50
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
    jmp near 02f2eh                           ; e9 66 42
int13_notfloppy:                             ; 0xfecc8 LB 0x14
    cmp dl, 0e0h                              ; 80 fa e0
    jc short 0ecdch                           ; 72 0f
    shr ebx, 010h                             ; 66 c1 eb 10
    push bx                                   ; 53
    call 040e4h                               ; e8 0f 54
    pop bx                                    ; 5b
    sal ebx, 010h                             ; 66 c1 e3 10
    jmp short 0ece9h                          ; eb 0d
int13_disk:                                  ; 0xfecdc LB 0xd
    cmp ah, 040h                              ; 80 fc 40
    jnbe short 0ece6h                         ; 77 05
    call 05427h                               ; e8 43 67
    jmp short 0ece9h                          ; eb 03
    call 05868h                               ; e8 7f 6b
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
    call 070e6h                               ; e8 09 81
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
    call 01765h                               ; e8 bb 26
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
    call 0474bh                               ; e8 7b 56
    inc sp                                    ; 44
    inc sp                                    ; 44
    test ax, ax                               ; 85 c0
    jne short 0f0feh                          ; 75 28
    mov ax, strict word 00002h                ; b8 02 00
    push ax                                   ; 50
    call 0474bh                               ; e8 6e 56
    inc sp                                    ; 44
    inc sp                                    ; 44
    test ax, ax                               ; 85 c0
    jne short 0f0feh                          ; 75 1b
    mov ax, strict word 00003h                ; b8 03 00
    push strict byte 00003h                   ; 6a 03
    call 0474bh                               ; e8 60 56
    inc sp                                    ; 44
    inc sp                                    ; 44
    test ax, ax                               ; 85 c0
    jne short 0f0feh                          ; 75 0d
    mov ax, strict word 00004h                ; b8 04 00
    push ax                                   ; 50
    call 0474bh                               ; e8 53 56
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
_pci_routing_table:                          ; 0xff2c0 LB 0x1e0
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
_pci_routing_table_size:                     ; 0xff4a0 LB 0x3a1
    loopne 0f4a3h                             ; e0 01
    times 0x39d db 0
    db  'XM'
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
int15_handler:                               ; 0xff859 LB 0x2e
    pushfw                                    ; 9c
    push DS                                   ; 1e
    push ES                                   ; 06
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    cmp ah, 086h                              ; 80 fc 86
    je short 0f88ch                           ; 74 28
    cmp ah, 0e8h                              ; 80 fc e8
    je short 0f88ch                           ; 74 23
    cmp ah, 0d0h                              ; 80 fc d0
    je short 0f88ch                           ; 74 1e
    pushaw                                    ; 60
    cmp ah, 053h                              ; 80 fc 53
    je short 0f882h                           ; 74 0e
    cmp ah, 0c2h                              ; 80 fc c2
    je short 0f887h                           ; 74 0e
    call 05f41h                               ; e8 c5 66
    popaw                                     ; 61
    pop ES                                    ; 07
    pop DS                                    ; 1f
    popfw                                     ; 9d
    jmp short 0f895h                          ; eb 13
    call 08f0bh                               ; e8 86 96
    jmp short 0f87ch                          ; eb f5
int15_handler_mouse:                         ; 0xff887 LB 0x5
    call 06d5bh                               ; e8 d1 74
    jmp short 0f87ch                          ; eb f0
int15_handler32:                             ; 0xff88c LB 0x9
    pushad                                    ; 66 60
    call 0640eh                               ; e8 7d 6b
    popad                                     ; 66 61
    jmp short 0f87dh                          ; eb e8
iret_modify_cf:                              ; 0xff895 LB 0x14
    jc short 0f8a0h                           ; 72 09
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
int74_handler:                               ; 0xff8a9 LB 0x2e
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
    call 06c91h                               ; e8 d4 73
    pop cx                                    ; 59
    jcxz 0f8cch                               ; e3 0c
    push strict byte 00000h                   ; 6a 00
    pop DS                                    ; 1f
    push word [0040eh]                        ; ff 36 0e 04
    pop DS                                    ; 1f
    call far [word 00022h]                    ; ff 1e 22 00
    cli                                       ; fa
    call 0e03bh                               ; e8 6b e7
    add sp, strict byte 00008h                ; 83 c4 08
    pop DS                                    ; 1f
    pop ES                                    ; 07
    popaw                                     ; 61
    iret                                      ; cf
int76_handler:                               ; 0xff8d7 LB 0x197
    push ax                                   ; 50
    push DS                                   ; 1e
    mov ax, strict word 00040h                ; b8 40 00
    mov ds, ax                                ; 8e d8
    mov byte [0008eh], 0ffh                   ; c6 06 8e 00 ff
    call 0e03bh                               ; e8 55 e7
    pop DS                                    ; 1f
    pop ax                                    ; 58
    iret                                      ; cf
    times 0x183 db 0
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
    db  080h, 0fch, 0b1h, 075h, 00fh, 006h, 01eh, 00eh, 01fh, 0fch, 066h, 060h, 0e8h, 09eh, 092h, 066h
    db  061h, 01fh, 007h, 0cfh, 006h, 01eh, 060h, 00eh, 01fh, 0fch, 0e8h, 09dh, 06bh, 061h, 01fh, 007h
    db  0cfh
int70_handler:                               ; 0xffe8f LB 0x16
    push ES                                   ; 06
    push DS                                   ; 1e
    pushaw                                    ; 60
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    call 0696ah                               ; e8 d2 6a
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
    push dx                                   ; 52
    mov ax, strict word 00040h                ; b8 40 00
    mov ds, ax                                ; 8e d8
    mov eax, dword [0006ch]                   ; 66 a1 6c 00
    inc eax                                   ; 66 40
    cmp eax, strict dword 0001800b0h          ; 66 3d b0 00 18 00
    jc short 0fec4h                           ; 72 07
    db  066h, 033h, 0c0h
    ; xor eax, eax                              ; 66 33 c0
    inc byte [word 00070h]                    ; fe 06 70 00
    mov dword [0006ch], eax                   ; 66 a3 6c 00
    mov AL, byte [00040h]                     ; a0 40 00
    db  00ah, 0c0h
    ; or al, al                                 ; 0a c0
    je short 0feddh                           ; 74 0e
    db  0feh, 0c8h
    ; dec al                                    ; fe c8
    mov byte [00040h], AL                     ; a2 40 00
    jne short 0feddh                          ; 75 07
    mov dx, 003f2h                            ; ba f2 03
    in AL, DX                                 ; ec
    and AL, strict byte 0cfh                  ; 24 cf
    out DX, AL                                ; ee
    int 01ch                                  ; cd 1c
    cli                                       ; fa
    call 0e03fh                               ; e8 5c e1
    pop dx                                    ; 5a
    pop DS                                    ; 1f
    pop eax                                   ; 66 58
    iret                                      ; cf
    times 0x9 db 0
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
    and ax, strict word 00000h                ; 25 00 00
    add byte [bx+si], dl                      ; 00 10
    push CS                                   ; 0e
    add byte [bx+si], al                      ; 00 00
    add byte [di], ah                         ; 00 25
    times 0x6f db 0
    db  'XM'
cpu_reset:                                   ; 0xffff0 LB 0x10
    jmp far 0f000h:0e05bh                     ; ea 5b e0 00 f0
    db  030h, 036h, 02fh, 032h, 033h, 02fh, 039h, 039h, 000h, 0fch, 0c9h
