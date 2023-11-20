using System.Collections.ObjectModel;

namespace HelloCloudWpf {
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
            models.Insert(index, item.Model);
            base.InsertItem(index, item);
        }

        protected override void SetItem(int index, TViewModel item) {
            models[index] = item.Model;
            base.SetItem(index, item);
        }
    }
}