import os
import glob

def process_line(line):
    # Split the line into components by commas
    parts = line.strip().split(',')

    # Unpack the required fields
    timestamp = parts[0]
    isSPI = parts[1]
    identifier = parts[2]
    isExtended = parts[3]
    isRemote = parts[4]
    length = int(parts[5])  # Convert length to an integer

    # Get the data fields
    data = parts[6:14]  # Data fields (up to 8)

    # Limit data to the specified length
    data = data[:length]

    # Combine all parts back into a formatted line
    new_line = f"{timestamp},{isSPI},{identifier},{isExtended},{isRemote},{length},{','.join(data)}"
    return new_line

def process_file(file_name):
    # Read the file and process each line
    with open(file_name, 'r', encoding='utf-16') as file:
        lines = file.readlines()

    # Process each line and update the content
    processed_lines = [process_line(line) for line in lines]

    # Write the processed lines back to the file
    with open(file_name, 'w', encoding='utf-8') as file:
        file.write('\n'.join(processed_lines) + '\n')

def process_all_utf16_csv_files():
    # Find all CSV files in the current directory with UTF-16 encoding
    csv_files = glob.glob("*.csv")

    for file_name in csv_files:
        try:
            # Attempt to open the file with UTF-16 encoding to confirm
            with open(file_name, 'r', encoding='utf-16') as file:
                # If successful, process the file
                print(f"Processing file: {file_name}")
                process_file(file_name)
        except UnicodeError:
            # If a Unicode error occurs, the file is likely not UTF-16, skip it
            print(f"Skipping non-UTF-16 file: {file_name}")

if __name__ == "__main__":
    process_all_utf16_csv_files()
