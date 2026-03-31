using OpenTCU.LoggingUtils;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Text;
using System.Windows.Controls;
using System.Windows.Data;

namespace OpenTCU.Visualiser
{
    internal class ViewModel
    {
        public ObservableCollection<SCanDump> Frames { get; set; }

        public ViewModel()
        {
            Frames = new();
            CollectionViewSource.GetDefaultView(Frames).SortDescriptions.Add(new SortDescription(nameof(SCanDump.Timestamp), ListSortDirection.Ascending));
        }
    }
}
