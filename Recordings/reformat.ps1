# Use the current working directory
$directory = Get-Location

# Loop through all .txt files in the current directory
Get-ChildItem -Path $directory -Filter "*.txt" | ForEach-Object {
    $filePath = $_.FullName
    $outputCsv = [System.IO.Path]::ChangeExtension($filePath, ".csv") # Change .txt to .csv

    # Clear or create the output CSV file
    Clear-Content -Path $outputCsv -ErrorAction SilentlyContinue

    # Read the content of the file and process each line
    Get-Content -Path $filePath | ForEach-Object {
        # Match lines containing the required pattern
        if ($_ -match "(\d{2}:\d{2}:\d{2}\.\d{3})\s+\e\[0;32mI.*CAN::Logger:\s+(.+?)\e\[0m") {
            $timestamp = $matches[1]
            $data = $matches[2]

            # Replace the first value with the timestamp
            $data = $data -replace "^\d+", $timestamp

            # Process the data: convert numeric values to hexadecimal, and the second value to ASCII
            $hexData = ($data -split ",") | ForEach-Object {
                # Index determines processing: timestamp, ASCII conversion, or hex
                $index = [Array]::IndexOf($data -split ",", $_)
                if ($index -eq 1 -and $_ -match "^\d+$") {
                    # Convert second value to ASCII if numeric and within valid range
                    if ([int]$_ -ge 32 -and [int]$_ -le 126) {
                        [char][int]$_
                    } else {
                        $_ # Leave as-is if not valid ASCII
                    }
                } elseif ($_ -match "^\d+$") {
                    "0x" + [Convert]::ToString([int]$_, 16) # Convert numeric to hex
                } else {
                    $_ # Leave non-numeric values unchanged
                }
            }

            # Join the processed data and write to the CSV file
            ($hexData -join ",") | Out-File -Append -FilePath $outputCsv
        }
    }

    Write-Host "Processed $filePath into $outputCsv"
}
