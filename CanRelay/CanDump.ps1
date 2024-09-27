# Create the folder if it doesn't exist
$folderPath = "./can_dumps"
if (-not (Test-Path $folderPath)) {
    New-Item -ItemType Directory -Path $folderPath
}

# Generate a random file name with a timestamp
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$randomFileName = "output_$timestamp.csv"
$outputFilePath = Join-Path $folderPath $randomFileName

# Display the generated file path
Write-Host "Filtering output to: $outputFilePath"

# Start the command and stream the output to the terminal
pio device monitor --environment s3_mini | ForEach-Object {
    # Check if the line contains "[CAN]"
    if ($_ -match "\[CAN\]") {
        # Strip everything to the left of "[CAN]"
        $filteredLine = $_ -replace "^.*\[CAN\]", "[CAN]"
        # Output the filtered line to the terminal
        $filteredLine | Write-Host
        # Append the filtered line to the file
        $filteredLine | Out-File -Append -FilePath $outputFilePath
    }
    else {
        # Output non-filtered lines to the terminal
        $_ | Write-Host
    }
}
