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
        # Strip everything including the "[CAN] " prefix
        $filteredLine = $_ -replace "^.*\[CAN\] ", ""

        # Split the message by commas
        $messageParts = $filteredLine -split ","

        # Replace the first entry (timestamp) with the local ISO timestamp
        $isoTimestamp = Get-Date -Format "yyyy-MM-ddTHH:mm:ss.fffZ"
        $messageParts[0] = $isoTimestamp

        # Join the message parts back together
        $updatedMessage = $messageParts -join ","

        # Output the updated message to the terminal
        $updatedMessage | Write-Host

        # Append the updated message to the file
        $updatedMessage | Out-File -Append -FilePath $outputFilePath
    }
    else {
        # Output non-filtered lines to the terminal
        $_ | Write-Host
    }
}
