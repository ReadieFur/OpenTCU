using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Windows;
using System.Windows.Controls;

namespace CANScrubber
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        class Row
        {
            public int timestamp { get; set; }
            public int id { get; set; }
            public string data { get; set; }
            public bool isSPI { get; set; }
            public bool isExtended { get; set; }
            public bool isRemote { get; set; }
            public int length { get; set; }
        }

        private string activeFile = null;
        private FileSystemWatcher fileWatcher = null;
        private ObservableCollection<Row> rows = new ObservableCollection<Row>();
        private double firstTimestamp = 0;
        private double lastTimestamp = 0;
        private List<Row> cache = new List<Row>();

        public MainWindow()
        {
            InitializeComponent();
            DG1.DataContext = rows;
        }

        private void Button_Click(object sender, RoutedEventArgs e)
        {
            //Open file picker
            Microsoft.Win32.OpenFileDialog dlg = new Microsoft.Win32.OpenFileDialog();
            dlg.DefaultExt = ".candump";
            dlg.Filter = "CAN Dump Files (*.candump)|*.candump";
            bool? result = dlg.ShowDialog();
            if (result == true)
            {
                fileWatcher?.Dispose();
                firstTimestamp = 0;
                lastTimestamp = 0;
                cache.Clear();
                Dispatcher.Invoke(() =>
                {
                    rows.Clear();
                    Title = dlg.FileName;
                });

                activeFile = dlg.FileName;
                fileWatcher = new FileSystemWatcher();
                fileWatcher.Path = Path.GetDirectoryName(dlg.FileName);
                fileWatcher.Filter = Path.GetFileName(dlg.FileName);
                fileWatcher.NotifyFilter = NotifyFilters.LastWrite;
                fileWatcher.Changed += FileWatcher_Changed;
                fileWatcher.EnableRaisingEvents = true;
                FileWatcher_Changed(null, null);
            }
        }

        private void FileWatcher_Changed(object sender, FileSystemEventArgs e)
        {
            try
            {
                // Open the file with read and write access, ignoring the fact that it's in use
                using (FileStream fileStream = new FileStream(activeFile, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
                using (StreamReader reader = new StreamReader(fileStream))
                    // Read the new content of the file
                    ProcessFrames(reader.ReadToEnd());
            }
            catch (IOException ex)
            {
                // Handle the IOException or any other exceptions
                Console.WriteLine($"Error reading file: {ex.Message}");
            }
        }

        private void ProcessFrames(string data)
        {
            cache.Clear();
            Dispatcher.Invoke(() => slider.Value = 100);

            List<Row> newRows = new List<Row>();
            foreach (string line in data.Split(new[] { '\n' }, StringSplitOptions.RemoveEmptyEntries))
            {
                string[] parts = line.Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries);
                if (parts.Length != 14)
                    continue;

                List<string> dataParts = new List<string>();
                for (int i = 6; i < int.Parse(parts[5]) + 6; i++)
                {
                    string value = parts[i];
                    //Data is in hex, pad with 0s where needed.
                    if (parts[i].Length == 1)
                        value = "0" + value;
                    dataParts.Add(value);
                }

                string hexData = string.Join(" ", dataParts).Replace("\r", "");
                long dataAsLong = long.Parse(string.Join("", dataParts), System.Globalization.NumberStyles.HexNumber);
                long[] longs = dataParts.Select(s => long.Parse(s, System.Globalization.NumberStyles.HexNumber)).ToArray();
                string dataString = $"#:{hexData}\nll:{string.Join(" ", longs)}\nl:{dataAsLong}";

                Row row = new Row()
                {
                    timestamp = int.Parse(parts[0]),
                    isSPI = parts[1] == "1",
                    id = int.Parse(parts[2], System.Globalization.NumberStyles.HexNumber),
                    isExtended = parts[3] == "1",
                    isRemote = parts[4] == "1",
                    length = int.Parse(parts[5]),
                    data = dataString
                };

                newRows.Add(row);
            }

            newRows.Sort((a, b) => a.timestamp.CompareTo(b.timestamp));
            cache.AddRange(newRows);

            foreach (Row row in newRows)
            {
                UpdateRow(row);

                if (row.timestamp < firstTimestamp)
                    firstTimestamp = row.timestamp;
                if (row.timestamp > lastTimestamp)
                    lastTimestamp = row.timestamp;
            }

            SortRows();
        }

        private void MoveToFrame(double index)
        {
            rows.Clear();
            
            List<int> ids = new List<int>();
            
            for (int i = 0; i < cache.Count * (int)index / 100; i++)
            {
                ids.Add(cache[i].id);
                UpdateRow(cache[i]);
            }

            ids = ids.Distinct().ToList();
            foreach (Row row in cache.Where(r => !ids.Contains(r.id)).GroupBy(r => r.id).Select(g => g.First()))
            {
                rows.Add(new Row()
                {
                    timestamp = 0,
                    isSPI = row.isSPI,
                    id = row.id,
                    isExtended = row.isExtended,
                    isRemote = row.isRemote,
                    length = row.length,
                });
            }

            SortRows();
        }

        //TODO: Add a "whats changed" column to make it easier to see what values have changed (or use colours).
        private void UpdateRow(Row row)
        {
            int existingIndex = rows.IndexOf(rows.FirstOrDefault(r => r.id == row.id));
            if (existingIndex == -1)
            {
                rows.Add(row);
                return;
            }

            if (rows[existingIndex].timestamp > row.timestamp)
                return;

            rows.RemoveAt(existingIndex);
            rows.Insert(existingIndex, row);
        }

        private void SortRows()
        {
            IReadOnlyList<Row> sortedRows = rows.OrderBy(r => r.id).ToList();
            rows.Clear();
            foreach (Row row in sortedRows)
            {
                //Clone the row.
                Row _row = new Row()
                {
                    timestamp = row.timestamp,
                    isSPI = row.isSPI,
                    id = row.id,
                    isExtended = row.isExtended,
                    isRemote = row.isRemote,
                    length = row.length,
                    data = row.data
                };

                if (decimalCheckbox.IsChecked == true)
                {
                    string[] data = _row.data.Split(new[] { ' ' }, StringSplitOptions.RemoveEmptyEntries);
#if true
                    for (int i = 0; i < data.Length; i++)
                        data[i] = int.Parse(data[i], System.Globalization.NumberStyles.HexNumber).ToString();
                    _row.data = string.Join(" ", data);
#else
                    string hexData = string.Join("", data);
                    if (long.TryParse(hexData, System.Globalization.NumberStyles.HexNumber, null, out long dataAsLong))
                        _row.data = dataAsLong.ToString();
                    else
                        _row.data = hexData;
#endif
                }

                rows.Add(_row);
            }
        }

        private void Slider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            if (fileWatcher is null)
                return;
            MoveToFrame(e.NewValue);
        }

        private void DG1_LoadingRow(object sender, DataGridRowEventArgs e)
        {
        }

        private void decimalCheckbox_Checked(object sender, RoutedEventArgs e)
        {
            MoveToFrame(slider.Value);
            SortRows();
        }
    }
}
