using System.IO;
using System.Collections.Generic;
using System.Collections.Specialized;

//https://closedxml.io/ClosedXML/

//Not the most memory efficent but for my use its fine. Ideal would be to load, process and discard line by line.
async Task ProcessFile(string filePath)
{
    Console.WriteLine($"Processing file: {filePath}");

    IReadOnlyList<string> lines = await File.ReadAllLinesAsync(filePath);

    IReadOnlyList<string> sourceColumns = lines.First().Split(',').Select(x => x.Replace("\"", "")).ToList();
    if (!new[] { "type", "start_time", "duration", "identifier", "num_data_bytes", "ack", "data" }.All(x => sourceColumns.Contains(x)))
    {
        Console.WriteLine("Missing required columns");
        return;
    }

    List<OrderedDictionary> newTable = new();
    OrderedDictionary currentRow = new();
    int currentRowDataCount = 0;
    for (int i = 1; i < lines.Count(); i++)
    {
        string line = lines.ElementAt(i);
        string[] values = line.Split(',');
        Dictionary<string, string> sourceRow = new();
        for (int j = 0; j < sourceColumns.Count; j++)
            sourceRow[sourceColumns[j]] = values[j].Replace("\"", "");

        switch (sourceRow["type"])
        {
            case "identifier_field":
                //This should always be the first field in a new set.
                //Don't flush the row, this should be done by ACK. If there was no ACK then the row was invalid.
                currentRow = new()
                {
                    { "Timestamp", "" },
                    { "IsSPI", "" },
                    { "ID", "" },
                    { "IsExtended", "" },
                    { "IsRemote", "" },
                    { "Length", "" },
                    { "D1", "" },
                    { "D2", "" },
                    { "D3", "" },
                    { "D4", "" },
                    { "D5", "" },
                    { "D6", "" },
                    { "D7", "" },
                    { "D8", "" },
                };
                currentRowDataCount = 0;

                currentRow["ID"] = string.Concat(sourceRow["identifier"].SkipWhile(c => c == '0' || c == 'x'));
                currentRow["Timestamp"] = sourceRow["start_time"];
                break;
            case "control_field":
                currentRow["Length"] = string.Concat(sourceRow["num_data_bytes"].SkipWhile(c => c == '0' || c == 'x'));
                break;
            case "ack_field":
                if (currentRow.Count > 0)
                {
                    newTable.Add(currentRow);
                    //Row reset is done in "identifier_field".
                }
                break;
            case "data_field":
                currentRowDataCount++;
                currentRow[$"D{currentRowDataCount}"] = string.Concat(sourceRow["data"].Skip(2));
                break;
            /*case "extended_field":
                currentRow["IsExtended"] = sourceRow["extended"];
                break;
            case "remote_field":
                currentRow["IsRemote"] = sourceRow["remote_frame"];
                break;*/
            default:
                break;
        }
    }

    using (StreamWriter writer = new(filePath.Replace("_raw.csv", ".csv"), false))
    {
        // writer.WriteLine(string.Join(',', newTable.First().Keys.Cast<string>()));
        foreach (OrderedDictionary row in newTable)
            writer.WriteLine(string.Join(',', row.Values.Cast<string>()));
    }
}

foreach (string file in Directory.GetFiles(Directory.GetCurrentDirectory(), "*_raw.csv"))
{
    await ProcessFile(file);
    File.Delete(file);
}
