using System.Text;

namespace HelloCloudWpf {
    public class TransferModel {
        public enum Direction { Send, Receive, Upload, Download }
        public enum Result { Success, Failure, Canceled }

        public Direction direction;
        public Result result;
        public string fileName;
        public string url;

        public TransferModel (Direction direction, string fileName, string url, Result result) {
            this.direction = direction;
            this.fileName = fileName;
            this.url = url;
            this.result = result;
        }

}

    public class TransferViewModel {
        public enum Direction { Send, Receive, Upload, Download }
        public enum Result { Success, Failure, Canceled }

        public Direction direction;
        public Result result;
        public string fileName;
        public string url;

        public TransferViewModel(Direction direction, string fileName, string url, Result result) {
            this.direction = direction;
            this.fileName = fileName;
            this.url = url;
            this.result = result;
        }

        public override string ToString() {
            StringBuilder sb = new();
            sb.Append(DirectionToChar(direction))
                .Append(ResultToChar(result))
                .Append(' ')
                .Append(fileName)
                .Append(" | ")
                .Append(url);
            return sb.ToString();
        }

        private static char DirectionToChar(Direction direction) {
            return direction switch {
                Direction.Send => '↖',
                Direction.Receive => '↘',
                Direction.Upload => '↑',
                Direction.Download => '↓',
                _ => '⚠',
            };
        }

        private static char ResultToChar(Result result) {
            return result switch {
                Result.Success => '✓',
                Result.Failure => '✕',
                Result.Canceled => '⚠',
                _ => ' ',
            };
        }
    }
}
