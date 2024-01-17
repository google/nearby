import Foundation

class DataWrapper<T: File>: Encodable, Decodable {
  enum DataContent {
    case notificationToken(String), packet(Packet<T>)
  }

  let data: DataContent

  init(notificationToken: String) {
    data = .notificationToken(notificationToken)
  }

  init(packet: Packet<T>) {
    data = .packet(packet)
  }

  required init(from decoder: Decoder) throws {
    let container = try decoder.container(keyedBy: CodingKeys.self)
    let type = try container.decode(String.self, forKey: .type)
    if type == "notificationToken" {
      let notificationToken = try container.decode(String.self, forKey: .data)
      self.data = .notificationToken(notificationToken)
    } else if type == "packet" {
      let packet = try container.decode(Packet<T>.self, forKey: .data)
      self.data = .packet(packet)
    } else {
      throw NSError(domain: "Decoding", code: 1)
    }
  }

  func encode(to encoder: Encoder) throws {
    var container = encoder.container(keyedBy: CodingKeys.self)
    switch data {
    case .notificationToken(let notificationToken):
      try container.encode("notificationToken", forKey: .type)
      try container.encode(notificationToken, forKey: .data)
    case .packet(let packet):
      try container.encode("packet", forKey: .type)
      try container.encode(packet, forKey: .data)
    }
  }

  enum CodingKeys: String, CodingKey {
    case type, data
  }
}
