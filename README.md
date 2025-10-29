# apc_injector
## Description

A demo that uses kernel APC to achieve user-mode DLL injection which references [Blackbone](https://github.com/DarthTon/Blackbone.git) and [KeInject](https://github.com/adrianyy/KeInject.git).

## Usage

1. Build

```cmd
@rem gererate and build Win32 project
rmdir /s /q build
cmake -G "Visual Studio 16 2019" -A Win32 -DTARGET_DIRECTORY=C:\Inject -B build -S .
cmake --build build --config RelWithDebInfo

@rem gererate and build x64 project when target arch is AMD64
rmdir /s /q build
cmake -G "Visual Studio 16 2019" -A x64 -DTARGET_DIRECTORY=C:\Inject -B build -S .
cmake --build build --config RelWithDebInfo
```

2. Deploy

```cmd
cd C:\Inject
@rem install driver
deployment -install
@rem uninstall driver if necessary
deployment -uninstall
```

3. Usage

```cmd
cd C:\Inject\x86
@rem specify a process ID
@rem if no error, means the injection successful
injCli -p 882

@rem specify a path
@rem all depends started from now on will attempt to inject
injcli -f C:\Users\Test\Desktop\Depends.exe
```

## Attention

Due to the special state of certain processes, the injected user-mode APC cannot execute immediately. 
In this state, attempting to unload the driver will block until the APC completes or the process exits.
You can view the log of the driver execution injection process through dbgview.

```
[HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Session Manager\Debug Print Filter]
"IHVDRIVER"=dword:0000000f
```

