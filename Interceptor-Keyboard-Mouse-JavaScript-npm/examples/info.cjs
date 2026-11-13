const {Interceptor}=require("interceptor-keyboard-mouse-sdk");const io=new Interceptor();try{console.log(io.info())}finally{io.close()}
