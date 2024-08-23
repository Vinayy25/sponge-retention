from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
import requests

app = FastAPI()

class EmailParams(BaseModel):
    to_email: str
    from_name: str
    subject: str
    sponge_count: int
    total_retention: int
    procedure_start_time: str
    procedure_end_time: str
    report_summary: str

EMAILJS_SERVICE_ID = "service_wjdh5hp"
EMAILJS_TEMPLATE_ID = "template_dp8x5t1"
EMAILJS_USER_ID = "bupyCZf6_lUBwwOUi"
EMAILJS_ACCESS_TOKEN = "DZHak3GqAShCugho5gtIr"

@app.post("/send-email/")
async def send_email(params: EmailParams):
    url = "https://api.emailjs.com/api/v1.0/email/send"
    headers = {
        "Content-Type": "application/json"
    }
    data = {
        "service_id": EMAILJS_SERVICE_ID,
        "template_id": EMAILJS_TEMPLATE_ID,
        "user_id": EMAILJS_USER_ID,
        "template_params": {
            "to_email": params.to_email,
            "from_name": params.from_name,
            "subject": params.subject,
            "sponge_count": str(params.sponge_count),
            "total_retention": str(params.total_retention),
            "procedure_start_time": params.procedure_start_time,
            "procedure_end_time": params.procedure_end_time,
            "report_summary": params.report_summary
        },
        "accessToken": EMAILJS_ACCESS_TOKEN
    }

    response = requests.post(url, json=data, headers=headers)

    if response.status_code == 200:
        return {"message": "Email sent successfully"}
    else:
        raise HTTPException(status_code=response.status_code, detail=response.text)

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
