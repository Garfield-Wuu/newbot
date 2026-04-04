from email.mime.text import MIMEText
from email.mime.image import MIMEImage
from email.mime.base import MIMEBase
from email.mime.multipart import MIMEMultipart
from email.header import Header
from email import encoders
import smtplib
import time
from email.utils import formataddr
import os

def send_mail(jpeg_data,str_data):
    email_host = 'smtp.qq.com'  # 服务器地址 163邮箱"smtp.163.com"  qq邮箱"smtp.qq.com"都需要开通smtp权限
    # sender = 'xxx@qq.com'  # 发件人（自己的邮箱）
    # password = 'xxx'  # 密码（授权码）
    # receiver = 'xxx@qq.com'  # 收件人

    # 下面的邮箱和授权码根据自己实际情况修改，授权码去QQ邮箱官网申请
    email_address = os.environ.get('EMAIL_ADDRESS')
    email_pwd = os.environ.get('EMAIL_PWD')
    if email_address == None or email_pwd == None:
        print("注意拍照前把自己的邮箱和授权码写到环境变量里:EMAIL_ADDRESS,EMAIL_PWD!")
        return -1

    sender = email_address  # 发件人（自己的邮箱）
    password = email_pwd  # 密码（授权码）
    receiver = email_address  # 收件人

    msg = MIMEMultipart()
    time_str = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(time.time()))
    msg['Subject'] = "Newbot " + time_str  # 标题
    msg['From'] = formataddr(["Newbot", sender])  # 括号里的对应发件人邮箱昵称、发件人邮箱账号
    msg['To'] = formataddr(["Master", receiver])  # 括号里的对应收件人邮箱昵称、收件人邮箱账号

    #     signature = '''
    # \n\t this is auto test report!
    # \n\t you don't need to follow
    # '''
    # text = MIMEText(signature, 'plain')  # 签名
    # msg.attach(text)

    # 正文-图片 只能通过html格式来放图片
    mail_msg = '''
    <p style="font-size:18px">%s</p>
    <p><img src="cid:my_image"></p>
    '''%(str_data)
    msg.attach(MIMEText(mail_msg, 'html', 'utf-8'))
    # 指定图片为当前目录
    #fp = open(image_path, 'rb')
    #jpeg_data = fp.read()
    msgImage = MIMEImage(jpeg_data)
    #fp.close()
    # 定义图片 ID，在 HTML 文本中引用
    msgImage.add_header('Content-ID', '<my_image>')
    msg.attach(msgImage)

    ctype = 'application/octet-stream'
    maintype, subtype = ctype.split('/', 1)

    # 附件-图片
    image = MIMEImage(jpeg_data, _subtype=subtype)
    image.add_header('Content-Disposition', 'attachment', filename=time_str+'.jpg')
    msg.attach(image)

    # 发送
    try:
        smtp = smtplib.SMTP_SSL(email_host, 465)
        # smtp = smtplib.SMTP()
        # smtp.connect(email_host, 25)
        smtp.login(sender, password)
        smtp.sendmail(sender, receiver, msg.as_string())
        smtp.quit()
        print('mail send success')
        return 0
    except:
        print('mail send fail')
        return -2

if __name__ == "__main__":
    fp = open("dog.jpg", 'rb')
    jpeg_data = fp.read()
    send_mail(jpeg_data)
    fp.close()

