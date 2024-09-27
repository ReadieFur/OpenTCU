using System.IO;
using System.Collections.Generic;

//Not the most memory efficent but for my use its fine. Ideal would be to load, process and discard line by line.
async Task ProcessSerialFile(string filePath)
{
    string fileDirectory = Directory.GetParent(filePath)!.FullName;
    string newFilePath = Path.Combine(fileDirectory, "can.csv");
    using (StreamWriter writer = new(newFilePath, false))
    {
        foreach (string line in await File.ReadAllLinesAsync(filePath))
        {
            int canStart = line.IndexOf("[can] ");
            if (canStart == -1)
                continue;

            string timestamp = line.Split(' ', 2)[0];
            string canData = line.Substring(canStart + 6);

            List<string> canDataParts = canData.Split(',').ToList();
            canDataParts[0] = timestamp;
            //Remove excess data (that exceeds the message length).
            for (int i = 6 + int.Parse(canDataParts[5]); i < canDataParts.Count; i++)
                canDataParts[i] = "";
            
            canData = string.Join(',', canDataParts);
            //Console.WriteLine(canData);
            await writer.WriteLineAsync(canData);
        }
    }
    //File.Delete(filePath);
    File.Move(filePath, Path.Combine(Directory.GetParent(filePath)!.FullName, "serial.txt"));
}

string directory = Directory.GetCurrentDirectory();
//directory = "D:\\Users\\ReadieFur\\Documents\\GitHub\\ReadieFur\\OpenTCU\\CAN Dumps\\0pct";
foreach (string file in Directory.GetFiles(directory, "*", SearchOption.AllDirectories))
{
    Console.WriteLine(file);
    string fileName = Path.GetFileName(file);
    if (fileName.EndsWith(".txt"))
        await ProcessSerialFile(file); //TODO: Auto-populate the xlsx file.
    else if (fileName.EndsWith(".mp4"))
        File.Move(file, Path.Combine(Directory.GetParent(file)!.FullName, "screen.mp4"));
    else if (fileName.EndsWith(".fit")) //TODO: Split fit file into csv files.
        File.Move(file, Path.Combine(Directory.GetParent(file)!.FullName, "ride.fit"));
}
