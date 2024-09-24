import csv
import glob
import os

def convert_to_trc(csv_file):
    trc_file = csv_file.replace('.csv', '.trc')
    
    with open(csv_file, 'r', encoding='utf-8') as infile, open(trc_file, 'w', encoding='utf-8') as outfile:
        reader = csv.DictReader(infile)
        outfile.write("Time ID DLC Data Comment\n")  # Header for the TRC file
        
        for row in reader:
            timestamp = row['Timestamp']
            can_id = row['ID']
            dlc = row['Length']
            data = [row[f'D{i}'] for i in range(1, 9)]  # Collect all data fields D1-D8
            
            # Trim the data fields to the length specified by DLC
            data = data[:int(dlc)]
            data_str = ' '.join(data).upper()  # Convert to upper case and join with spaces
            
            # Write the formatted line to the TRC file
            outfile.write(f"{timestamp} {can_id} {dlc} {data_str} \n")

def convert_all_csv_to_trc():
    csv_files = glob.glob("*.csv")
    
    for csv_file in csv_files:
        print(f"Converting {csv_file} to TRC format...")
        convert_to_trc(csv_file)

if __name__ == "__main__":
    convert_all_csv_to_trc()
