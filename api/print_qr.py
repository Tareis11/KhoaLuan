from db import db
from fastapi import APIRouter
from pydantic import BaseModel

router = APIRouter()


class PrintQRRequest(BaseModel):
    locker_id: int


@router.get("/api/print_qr")
async def get_print_qr():
    job = await db.print_queue.find_one_and_update(
        {"printed": False}, {"$set": {"printed": True}}
    )
    if job:
        return {
            "locker_id": job.get("locker_id"),
            "code": job.get("code"),
            "printed": True,
        }
    return {"locker_id": None, "code": None, "printed": False}


@router.post("/api/print_qr")
async def add_print_qr(data: PrintQRRequest):
    exists = await db.print_queue.find_one(
        {"locker_id": data.locker_id, "printed": False}
    )
    if exists:
        return {"status": "exists"}
    locker = await db.locker.find_one({"number": data.locker_id})
    if not locker or not locker.get("code"):
        return {"error": "Locker or code not found"}
    await db.print_queue.insert_one(
        {"locker_id": data.locker_id, "code": locker["code"], "printed": False}
    )
    return {"status": "added"}
