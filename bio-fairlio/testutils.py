
def byteval( s ):
    import re

    ''' Convert str of 1m, 1k, 1g to real byte values'''
    m = re.match(r"(\d+)(\w?)", str(s) )
    item = m.groups()

    # no suffix
    if len(item[1]) == 0:
        return int(item[0])
    elif item[1] == 'k' or item[1] == 'K':
        return int(item[0])*1024
    elif item[1] == 'm' or item[1] == 'M':
        return int(item[0])*1024*1024
    elif item[1] == 'g' or item[1] == 'G':
        return int(item[0])*1024*1024*1024
    else:
        raise ValueError("unknown value specified: %s" % s)

def read_config(config_file):
    import yaml
    import argparse

    config = {}
    try:
        with file(config_file) as f:
            g = yaml.safe_load_all(f)
            for new in g:
                config.update(new)
    except IOError, e:
        raise argparse.ArgumentTypeError(str(e))

    return config

def timestamp():
    import time

    return time.strftime("%Y-%m%d-%H%M%S", time.localtime())

def list2str(alist):
    """ convert a list of a string"""
    return ",".join(alist)


#
# see SO thread
# http://stackoverflow.com/questions/3362600/how-to-send-email-attachments-with-python

def send_mail(send_from, send_to, subject, text, files=[], server="localhost"):
    """
    text - message body

    For example:
    send_mail("fwang2", ["fwang2@gmail.com"], "test ...", "okay", files=["bio.py"])

    """
    import smtplib, os
    from email.MIMEMultipart import MIMEMultipart
    from email.MIMEBase import MIMEBase
    from email.MIMEText import MIMEText
    from email.Utils import COMMASPACE, formatdate
    from email import Encoders

    assert type(send_to)==list
    assert type(files)==list

    msg = MIMEMultipart()
    msg['From'] = send_from
    msg['To'] = COMMASPACE.join(send_to)
    msg['Date'] = formatdate(localtime=True)
    msg['Subject'] = subject

    msg.attach( MIMEText(text) )

    for f in files:
        part = MIMEBase('application', "octet-stream")
        part.set_payload( open(f,"rb").read() )
        Encoders.encode_base64(part)
        part.add_header('Content-Disposition', 'attachment; filename="%s"' % os.path.basename(f))
        msg.attach(part)

    smtp = smtplib.SMTP(server)
    smtp.sendmail(send_from, send_to, msg.as_string())
    smtp.close()

def wrap(s, n=80):
    """
    wrap and return a string within specified width
    """
    res = [ ]

    if len(s) > n:
        num = int(len(s)/n)
        for i in range(num):
            res.append(s[i*n:(i+1)*n])
        res.append(s[num*n:])

        return "\t\n".join(res)

    else:
        return s

def sig_handler(signal, frame):
    import sys

    print "\tUser cancelled!"
    sys.exit(0)

