
const grpc = require('@grpc/grpc-js')
const message_proto = require('./proto')
const const_module = require('./const')
//const { v4: uuidv4 } = require('uuid');
const emailModule = require('./email');
const redis_module = require('./redis')

/**
 * GetVerifyCode grpc响应获取验证码的服务
 * @param {*} call 为grpc请求 
 * @param {*} callback 为grpc回调
 * @returns 
 */
async function GetVerifyCode(call, callback) {
    console.log("email is ", call.request.email)
    try{
        let uniqueId;
        let query_res = await redis_module.GetRedis(const_module.code_prefix+call.request.email);
        console.log("query_res is ", query_res)

        if(query_res ==null)
        {
            // uuid v13 是 ESM-only：用动态 import
            const { v4: uuidv4 } = await import('uuid');
            uniqueId = uuidv4().slice(0, 4); // 可选：只取前4位

            let bres = await redis_module.SetRedisExpire(const_module.code_prefix+call.request.email, uniqueId, 600)
            if(!bres){
                callback(null, { email:  call.request.email,
                    error:const_module.Errors.RedisErr
                });
                return;
            }
        }
        else
        {
            uniqueId = query_res;
        }

        console.log("uniqueId is ", uniqueId)
        let text_str =  'your verify code is\n'+ uniqueId +'\nplease complete registration within three minutes'
        //发送邮件
        let mailOptions = {
            from: 'ysk4612@163.com',
            to: call.request.email,
            subject: 'verify code',
            text: text_str,
        };
    
        let send_res = await emailModule.SendMail(mailOptions);
        console.log("send res is ", send_res)

        if(!send_res)
        {
            callback(null, { email:  call.request.email,
                error:const_module.Errors.RedisErr
            }); 
            return;
        }

        callback(null, { email:  call.request.email,
            error:const_module.Errors.Success
        }); 
        
 
    }catch(error){
        console.log("catch error is ", error)

        callback(null, { email:  call.request.email,
            error:const_module.Errors.Exception
        }); 
    }
     
}

function main() {
    var server = new grpc.Server()
    server.addService(message_proto.VerifyService.service, { GetVerifyCode: GetVerifyCode })
    server.bindAsync('0.0.0.0:50051', grpc.ServerCredentials.createInsecure(), () => {
        server.start()
        console.log('grpc server started')        
    })
}

main()