const path = require('path')
const grpc = require('@grpc/grpc-js')
const protoLoader = require('@grpc/proto-loader')

const PROTO_PATH = path.join(__dirname, 'message.proto')
const packageDefinition = protoLoader.loadSync(PROTO_PATH, { keepCase: true, longs: String, enums: String, defaults: true, oneofs: true })
const protoDescriptor = grpc.loadPackageDefinition(packageDefinition)

const message_proto = protoDescriptor.message

module.exports = message_proto

/*开启IMAP/SMTP
成功开启IMAP/SMTP服务，在第三方客户端登录时，登录密码输入以下授权密码
ZVhk34kVPPv87U9K

授权码只显示1次，有效期180天

使用设备 
如办公电脑、手机客户端、家庭电脑等
*/