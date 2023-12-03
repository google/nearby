using System.Text;

namespace HelloCloudWpf {
    public class TransferModel {
        public enum Direction { Send, Receive, Upload, Download }
        public enum Result { Success, Failure, Canceled }

        public Direction direction;
        public Result result;
        public string remotePath;

        public TransferModel (Direction direction, string url, Result result) {
            this.direction = direction;
            this.remotePath = url;
            this.result = result;
        }
}

    public class TransferViewModel : IViewModel<TransferModel> {
        public TransferModel? Model { get; set; }

        public TransferViewModel() { }

        public override string ToString() {
            StringBuilder sb = new();
            sb.Append(DirectionToChar(Model!.direction))
                .Append(ResultToChar(Model!.result))
                .Append(' ')
                .Append(Model.remotePath);
            return sb.ToString();
        }

        private static char DirectionToChar(TransferModel.Direction direction) {
            return direction switch {
                TransferModel.Direction.Send => '↖',
                TransferModel.Direction.Receive => '↘',
                TransferModel.Direction.Upload => '↑',
                TransferModel.Direction.Download => '↓',
                _ => '⚠',
            };
        }

        private static char ResultToChar(TransferModel.Result result) {
            return result switch {
                TransferModel.Result.Success => '✓',
                TransferModel.Result.Failure => '✕',
                TransferModel.Result.Canceled => '⚠',
                _ => ' ',
            };
        }
    }
}
