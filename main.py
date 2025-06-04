from api import init
from api import lockers
from api import unlock
from api import toggle
from api import history
from api import reset
from api import request
from api import temperature
from fastapi import FastAPI
from dotenv import load_dotenv  # ⚠️ thêm dòng này
from fastapi import Request
from fastapi.responses import HTMLResponse
from fastapi.templating import Jinja2Templates


load_dotenv()  # ⚠️ và dòng này


app = FastAPI()
app.include_router(init.router)
app.include_router(unlock.router)
app.include_router(toggle.router)
app.include_router(history.router)
app.include_router(reset.router)
app.include_router(request.router)
app.include_router(temperature.router)
app.include_router(lockers.router)


templates = Jinja2Templates(directory="templates")


@app.get("/", response_class=HTMLResponse)
async def read_root(request: Request):
    return templates.TemplateResponse("index.html", {"request": request})
