using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Threading.Tasks;

namespace ReadieFur.OpenTCU.Client
{
    public class MainPageViewModel : INotifyPropertyChanged
    {
        private int _baseCircumference = 2160;

        public event PropertyChangedEventHandler? PropertyChanged;

        public int BaseCircumference
        {
            get => _baseCircumference;
            set
            {
                if (BaseCircumference != value)
                {
                    //Don't check value ranges here as it can cause issues with text input, only validate it externally upon final submission.
                    /*if (value > 2400)
                        _baseCircumference = 2400;
                    else if (value < 2000)
                        _baseCircumference = 2000;
                    else*/
                        _baseCircumference = value;
                    OnPropertyChanged();
                }
            }
        }

        protected virtual void OnPropertyChanged([CallerMemberName] string? propertyName = null)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }
}
