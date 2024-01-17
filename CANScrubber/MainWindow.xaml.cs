using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using Newtonsoft.Json;

namespace CANScrubber
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        class Json
        {
            public double timestamp { get; set; }
            public int id { get; set; }
            public List<string> data { get; set; }
        }

        class Row
        {
            public int timestamp { get; set; }
            public int id { get; set; }
            public string data { get; set; }
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
            dlg.DefaultExt = ".dump";
            dlg.Filter = "CAN Dump Files (*.dump)|*.dump";
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
                Json json = JsonConvert.DeserializeObject<Json>(line);
                if (json is null)
                    continue;

                newRows.Add(new Row()
                {
                    timestamp = TimeSpan.FromMilliseconds(json.timestamp * 1000).Milliseconds,
                    id = json.id,
                    data = string.Join(" ", json.data)
                });
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
                    id = row.id,
                    data = string.Empty
                });
            }

            SortRows();
        }

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
                rows.Add(row);
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
    }
}
