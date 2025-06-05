from dotenv import load_dotenv

load_dotenv()
from fastapi import FastAPI, Request
from fastapi.responses import HTMLResponse
from fastapi.templating import Jinja2Templates

from api import (
    init,
    lockers,
    unlock,
    toggle,
    history,
    reset,
    request,
    temperature,
)

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
