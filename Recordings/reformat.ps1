# Define the mask as a global variable
$mask = @(0, 0, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1)

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
        if ($_ -match "^([\d:]+\.\d+)\sCAN::Logger:(.+)") {
            $timestamp = $matches[1]    # Extract logged timestamp
            $rawData = $matches[2]     # Extract raw data after CAN::Logger:
            $data = $rawData -split "," # Split data into an array

            # Replace data1 (index 0) with the logged timestamp (optional)
            # Uncomment the line below to enable timestamp replacement
            # $data[0] = $timestamp

            # Convert the second value (index 1) to ASCII
            if ($data[1] -as [int]) {
                $data[1] = [char]([int]$data[1] - 1) # Convert numeric value to its ASCII character
            }

            # Apply the mask and convert to hex where the mask value is 1
            $processedData = for ($i = 0; $i -lt $data.Length; $i++) {
                if ($mask[$i] -eq 1) {
                    [Convert]::ToString([int]$data[$i], 16).ToUpper() # Convert to uppercase hex
                } else {
                    $data[$i] # Keep the original value
                }
            }

            # Join the processed data and write to the output file
            ($processedData -join ",") | Out-File -Append -FilePath $outputCsv
        }
    }

    Write-Host "Processed $filePath into $outputCsv"
}
