from dotenv import load_dotenv
from fastapi.staticfiles import StaticFiles

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
    print_qr,
)


app = FastAPI()
app.include_router(print_qr.router)
app.include_router(init.router)
app.include_router(unlock.router)
app.include_router(toggle.router)
app.include_router(history.router)
app.include_router(reset.router)
app.include_router(request.router)
app.include_router(temperature.router)
app.include_router(lockers.router)
app.mount("/styles", StaticFiles(directory="styles"), name="styles")
app.mount("/images", StaticFiles(directory="images"), name="images")  # Thêm dòng này

templates = Jinja2Templates(directory="templates")


@app.get("/", response_class=HTMLResponse)
async def read_root(request: Request):
    return templates.TemplateResponse("index.html", {"request": request})


@app.get("/history")
async def history_page(request: Request):
    return templates.TemplateResponse("history.html", {"request": request})
