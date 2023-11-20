using System;
using System.Collections.ObjectModel;
using System.Windows.Input;

namespace HelloCloudWpf {
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

    public interface IViewModel<TModel> {
        public TModel? Model { get; set; }
    }

    public class ViewModelCollection<TViewModel, TModel> : ObservableCollection<TViewModel>
        where TViewModel : IViewModel<TModel>, new() {
        private readonly Collection<TModel> models;

        public ViewModelCollection(Collection<TModel> models) {
            this.models = models;
            foreach (TModel model in models) {
                TViewModel viewModel = new() {
                    Model = model
                };
                Add(viewModel);
            }
        }

        protected override void ClearItems() {
            models.Clear();
            base.ClearItems();
        }

        protected override void RemoveItem(int index) {
            models.RemoveAt(index);
            base.RemoveItem(index);
        }

        protected override void InsertItem(int index, TViewModel item) {
            models.Insert(index, item.Model!);
            base.InsertItem(index, item);
        }

        protected override void SetItem(int index, TViewModel item) {
            models[index] = item.Model!;
            base.SetItem(index, item);
        }
    }
}