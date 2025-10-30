\# Simplified File System (ZOS 2025)



A semester project for the course \*\*Základy operačních systémů (ZOS 2025)\*\*.  

The project implements a \*\*simplified file system based on i-nodes\*\*, written in \*\*C\*\*.



---



\## 📂 Overview



The program simulates a basic file system stored inside a single file on disk.  

It supports standard file and directory operations similar to those in UNIX systems.



Each command manipulates the virtual file system, not the host OS filesystem.



---



\## 🚀 Features



\### Core Commands

| Command | Description |

|----------|-------------|

| `cp s1 s2` | Copy file `s1` to location `s2`. |

| `mv s1 s2` | Move or rename a file. |

| `rm s1` | Delete a file. |

| `mkdir a1` | Create a directory. |

| `rmdir a1` | Remove an empty directory. |

| `ls \[a1]` | List directory contents. |

| `cat s1` | Print the contents of a text file. |

| `cd a1` | Change current working directory. |

| `pwd` | Print the current path. |

| `info s1` | Show file or directory metadata (name, size, i-node number, clusters). |

| `incp s1 s2` | Import a file from the host system to the virtual FS. |

| `outcp s1 s2` | Export a file from the virtual FS to the host system. |

| `load s1` | Load and execute commands from a file. |

| `format <size>` | Initialize the file system (e.g. `format 600MB`). |

| `statfs` | Display file system statistics. |

| `exit` | Exit the program. |



\### Extended Features (depending on student login range)

\- \*\*Hard link\*\* (`ln s1 s2`)

\- \*\*Concatenation / append\*\* (`xcp s1 s2 s3`, `add s1 s2`)

\- \*\*Symbolic link\*\* (`slink s1 s2`)



---



\## ⚙️ Implementation Details



\- Written in \*\*C (C99 standard)\*\*.

\- Uses \*\*i-node-based structure\*\* for file and directory management.

\- Each filename is fixed to \*\*12 bytes\*\* (`8.3` format + null terminator).

\- Commands are processed interactively from `stdin`.

\- The file system image is a binary file provided as the first program argument.



Example:

```bash

./filesystem myfs.dat



