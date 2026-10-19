#
# IotaOS - setup-wsl-usb.ps1
# Copyright (c) 2026 grish-ka
# Licensed under the MIT License.
#
# Configures a dedicated physical USB port for WSL bare-metal flashing.
# Run once with Administrator privileges on Windows.
#

# --- 1. Require Administrator & Fix WSL UNC Path Elevation ---
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "Elevating to Administrator..." -ForegroundColor Yellow
    # When launched from WSL, the path is a UNC path (\\wsl.localhost\...)
    # Elevated Windows processes cannot access UNC paths without special credentials.
    # Copy to Windows %TEMP% (C:\Users\...\AppData\Local\Temp) so it runs locally on C:
    $tempScript = Join-Path $env:TEMP "setup-wsl-usb.ps1"
    try {
        Copy-Item -Path $PSCommandPath -Destination $tempScript -Force
    } catch {
        # Fallback if $PSCommandPath is empty
        $tempScript = "C:\Users\grish\AppData\Local\Temp\setup-wsl-usb.ps1"
    }
    # Launch with -NoExit so if anything goes wrong, the window stays open to show the error
    Start-Process powershell -Verb RunAs -ArgumentList "-NoProfile -ExecutionPolicy Bypass -NoExit -File `"$tempScript`""
    exit
}

try {
    Clear-Host
    Write-Host "============================================================" -ForegroundColor Cyan
    Write-Host "           IotaOS: Dedicated WSL USB Port Setup             " -ForegroundColor Cyan
    Write-Host "============================================================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "This script configures ONE physical USB port on your PC as a"
    Write-Host "dedicated WSL flashing port using Microsoft's usbipd-win."
    Write-Host ""

    # --- 2. Check / Install usbipd-win ---
    $usbipdCmd = Get-Command usbipd -ErrorAction SilentlyContinue
    if (-not $usbipdCmd) {
        Write-Host "[1/4] Installing 'usbipd-win' via winget..." -ForegroundColor Yellow
        
        # Ensure winget can be found even in elevated context
        $wingetCmd = Get-Command winget -ErrorAction SilentlyContinue
        if (-not $wingetCmd) {
            $userWinget = "C:\Users\grish\AppData\Local\Microsoft\WindowsApps\winget.exe"
            if (Test-Path $userWinget) {
                & $userWinget install --id dorssel.usbipd-win -e --accept-source-agreements --accept-package-agreements
            } else {
                Write-Host "winget not found. Downloading usbipd installer directly..." -ForegroundColor Yellow
                $msiUrl = "https://github.com/dorssel/usbipd-win/releases/download/v5.3.0/usbipd-win_5.3.0.msi"
                $msiPath = "$env:TEMP\usbipd-win.msi"
                Invoke-WebRequest -Uri $msiUrl -OutFile $msiPath
                Start-Process msiexec.exe -ArgumentList "/i `"$msiPath`" /quiet /norestart" -Wait
            }
        } else {
            winget install --id dorssel.usbipd-win -e --accept-source-agreements --accept-package-agreements
        }
        
        # Refresh PATH in current session
        $env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")
        if (Test-Path "C:\Program Files\usbipd-win\usbipd.exe") {
            $env:Path += ";C:\Program Files\usbipd-win"
        }
    } else {
        Write-Host "[1/4] usbipd-win is already installed." -ForegroundColor Green
    }

    # Verify usbipd is callable
    $usbipdBin = Get-Command usbipd -ErrorAction SilentlyContinue
    if (-not $usbipdBin) {
        if (Test-Path "C:\Program Files\usbipd-win\usbipd.exe") {
            $usbipdBin = "C:\Program Files\usbipd-win\usbipd.exe"
        } else {
            throw "usbipd-win was installed but 'usbipd.exe' was not found in PATH or 'C:\Program Files\usbipd-win'."
        }
    }

    # --- 3. Safety Warning Banner ---
    Write-Host ""
    Write-Host "******************************** CRITICAL WARNING ********************************" -ForegroundColor Red
    Write-Host "* DO NOT BIND YOUR MOUSE, KEYBOARD, WEBCAM, HEADSET, OR BLUETOOTH ADAPTER!      *" -ForegroundColor Yellow
    Write-Host "*                                                                                *" -ForegroundColor Yellow
    Write-Host "* Binding a port passes that physical USB socket exclusively to WSL.             *" -ForegroundColor Yellow
    Write-Host "* While attached to WSL, Windows CANNOT see or use anything plugged into it.     *" -ForegroundColor Yellow
    Write-Host "*                                                                                *" -ForegroundColor Yellow
    Write-Host "* MAKE SURE YOU ONLY BIND THE PORT WHERE YOUR USB FLASH DRIVE IS CURRENTLY IN!   *" -ForegroundColor Yellow
    Write-Host "**********************************************************************************" -ForegroundColor Red
    Write-Host ""

    # --- 4. Detect Connected USB Devices ---
    Write-Host "[2/4] Scanning connected USB devices..." -ForegroundColor Yellow
    Write-Host "------------------------------------------------------------" -ForegroundColor DarkGray
    & $usbipdBin list
    Write-Host "------------------------------------------------------------" -ForegroundColor DarkGray

    # Detect Mass Storage devices to assist the user
    $storageDevices = Get-CimInstance Win32_PnPEntity -ErrorAction SilentlyContinue | Where-Object { $_.Service -eq 'USBSTOR' }
    if ($storageDevices) {
        Write-Host "`nDetected USB Flash Drive(s):" -ForegroundColor Green
        foreach ($dev in $storageDevices) {
            Write-Host "  -> $($dev.Name)" -ForegroundColor Cyan
            Write-Host "     Hardware ID: $($dev.DeviceID)" -ForegroundColor DarkGray
        }
    }

    Write-Host ""
    Write-Host "Look at the 'usbipd list' table above:" -ForegroundColor White
    Write-Host "Find the BUSID (e.g. 1-3, 2-1, 1-4) corresponding to your USB Flash Drive." -ForegroundColor White
    Write-Host ""

    $busid = Read-Host "Enter the BUSID for your USB Flash Drive port (or type 'cancel' to exit)"

    if ([string]::IsNullOrWhiteSpace($busid) -or $busid.Trim().ToLower() -eq 'cancel') {
        Write-Host "`nSetup cancelled. No ports or devices were modified." -ForegroundColor Yellow
        Write-Host "Press any key to exit..."
        [void][System.Console]::ReadKey()
        exit
    }

    $busid = $busid.Trim()

    # --- 5. Verify the selected device description for safety ---
    $rawList = & $usbipdBin list
    $selectedLine = $rawList | Where-Object { $_ -match "^\s*$([regex]::Escape($busid))\s+" }

    if ($selectedLine) {
        Write-Host "`nSelected: $selectedLine" -ForegroundColor Cyan
        
        # Check for obvious input devices
        if ($selectedLine -match "(Keyboard|Mouse|HID|Audio|Camera|Bluetooth|Wireless Receiver)") {
            Write-Host "`n[DANGER] The selected device appears to be an INPUT or AUDIO device!" -ForegroundColor Red
            Write-Host "Binding this device will cause it to STOP WORKING in Windows!" -ForegroundColor Red
            $override = Read-Host "Are you 100% sure you want to proceed? (Type 'I_KNOW_WHAT_I_AM_DOING' to continue)"
            if ($override -ne "I_KNOW_WHAT_I_AM_DOING") {
                Write-Host "Aborted for safety." -ForegroundColor Yellow
                Write-Host "Press any key to exit..."
                [void][System.Console]::ReadKey()
                exit
            }
        }
    }

    # --- 6. Confirm and Bind ---
    Write-Host ""
    Write-Host "Ready to bind BUSID '$busid' to WSL." -ForegroundColor Yellow
    Write-Host "Remember: You can still use this flash drive in Windows anytime by" -ForegroundColor White
    Write-Host "simply plugging it into a DIFFERENT USB port on your PC." -ForegroundColor White
    Write-Host ""

    $confirm = Read-Host "Type 'yes' to bind this port to WSL"
    if ($confirm.Trim().ToLower() -ne 'yes') {
        Write-Host "Aborted. No changes made." -ForegroundColor Yellow
        Write-Host "Press any key to exit..."
        [void][System.Console]::ReadKey()
        exit
    }

    Write-Host "`n[3/4] Binding port $busid with usbipd..." -ForegroundColor Yellow
    & $usbipdBin bind --busid $busid --force

    # --- 7. Configure Auto-Attach ---
    Write-Host "`n[4/4] Activating auto-attach for WSL..." -ForegroundColor Yellow
    & $usbipdBin attach --wsl --auto-attach --busid $busid

    Write-Host ""
    Write-Host "============================================================" -ForegroundColor Green
    Write-Host "                   SETUP COMPLETE!                          " -ForegroundColor Green
    Write-Host "============================================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "What happens now:" -ForegroundColor White
    Write-Host "  1. Whenever you plug ANY flash drive into this physical port," -ForegroundColor Cyan
    Write-Host "     Windows will automatically pass it into WSL as a raw disk." -ForegroundColor Cyan
    Write-Host "  2. In WSL, run './tools/flash-usb.sh' to flash IotaOS directly." -ForegroundColor Cyan
    Write-Host "  3. If you want Windows Explorer to see the drive, plug it into" -ForegroundColor Cyan
    Write-Host "     any other USB port on your PC." -ForegroundColor Cyan
    Write-Host ""
    Write-Host "How to UNDO this later (if you ever change your mind):" -ForegroundColor DarkGray
    Write-Host "  In an Admin PowerShell prompt, run:" -ForegroundColor DarkGray
    Write-Host "    usbipd unbind --busid $busid" -ForegroundColor DarkGray
    Write-Host ""

} catch {
    Write-Host "`n[ERROR] An error occurred:" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    Write-Host $_.ScriptStackTrace -ForegroundColor DarkRed
}

Write-Host "`nPress any key to exit..."
[void][System.Console]::ReadKey()
