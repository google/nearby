using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Windows;
using System.Windows.Input;

namespace HelloFirebaseWpf {
    public class RelayCommand : ICommand {
        readonly Action<object?> execute;
        readonly Predicate<object?> canExecute;

        public RelayCommand(Action<object?> execute, Predicate<object?> canExecute) {
            this.execute = execute ?? throw new ArgumentNullException(nameof(execute));
            this.canExecute = canExecute;
        }

        public bool CanExecute(object? parameter) {
            return canExecute == null || canExecute(parameter);
        }

        public event EventHandler? CanExecuteChanged {
            add { CommandManager.RequerySuggested += value; }
            remove { CommandManager.RequerySuggested -= value; }
        }

        public void Execute(object? parameter) { execute(parameter); }
    }

    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window, INotifyPropertyChanged {
        public event PropertyChangedEventHandler? PropertyChanged;
        public event EventHandler? CanExecuteChanged;

        public IEnumerable<string> PickedFileNames { get => openFileDialog.FileNames; }
        public ICommand PickFilesCommand { get => pickFilesCommand; }

        private readonly ICommand pickFilesCommand;
        private readonly OpenFileDialog openFileDialog;

        public MainWindow() {
            openFileDialog = new() { Multiselect = true };
            pickFilesCommand = new RelayCommand(
                _ => PickFiles(),
                _ => true);
            InitializeComponent();
        }

        private void PickFiles() {
            openFileDialog.FileName = string.Empty;
            bool? result = openFileDialog.ShowDialog();
            if (result == true) {
                Console.Write(openFileDialog.FileName);
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(PickedFileNames)));
            } 
        }
    }
}
