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
        # Match lines containing only raw data logs
        if ($_ -match "CAN::Logger:(\d+,\d+,\w+,.+)") {
            $rawData = $matches[1]  # Extract the raw data after CAN::Logger:
            # Write the raw data to the output file
            $rawData | Out-File -Append -FilePath $outputCsv
        }
    }

    Write-Host "Processed $filePath into $outputCsv"
}
