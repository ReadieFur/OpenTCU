# Define the source and destination
$inputFolder = "./"
$outputFolder = "./Corrected"

if (!(Test-Path $outputFolder)) { New-Item -ItemType Directory -Path $outputFolder }

$files = Get-ChildItem -Path $inputFolder -Filter "*.csv"

foreach ($file in $files) {
    Write-Host "Processing $($file.Name)..." -ForegroundColor Cyan
    
    $content = Get-Content $file.FullName | ForEach-Object {
        $parts = $_.Split(',')
        
        # 1. Remap the second column (Index 1)
        if ($parts.Count -gt 1) {
            if ($parts[1] -eq "49") { $parts[1] = "0" }
            elseif ($parts[1] -eq "50") { $parts[1] = "1" }
        }

        # 2. Convert Index 2 (The ID) from Decimal to Hex
        if ($parts.Count -gt 2 -and $parts[2] -match '^\d+$') {
            $parts[2] = "{0:X}" -f [int]$parts[2]
        }

        # 3. Convert Data Bytes starting at Index 6 (shifted one sooner)
        # We go from Index 6 to Index 13 (8 bytes total)
        $upperLimit = [Math]::Min($parts.Count - 1, 13)
        if ($parts.Count -gt 6) {
            for ($i = 6; $i -le $upperLimit; $i++) {
                if ($parts[$i] -match '^\d+$') {
                    $parts[$i] = "{0:X2}" -f [int]$parts[$i]
                }
            }
        }

        $parts -join ','
    }

    $content | Out-File -FilePath (Join-Path $outputFolder $file.Name) -Encoding utf8
}

Write-Host "Done!" -ForegroundColor Green
