from db import db
from fastapi import APIRouter

router = APIRouter()


@router.get("/api/print_qr")
async def get_print_qr():
    jobs = await db.print_queue.find({"printed": False}).to_list(length=None)
    result = []
    for job in jobs:
        result.append(
            {
                "locker_id": job.get("locker_id"),
                "code": job.get("code"),
                "printed": job.get("printed", False),
            }
        )
    return {"print_qr": result}


@router.post("/api/print_qr")
async def add_print_qr(locker_id: int):
    locker = await db.locker.find_one({"number": locker_id})
    if not locker or not locker.get("code"):
        return {"error": "Locker or code not found"}
    await db.print_queue.insert_one(
        {"locker_id": locker_id, "code": locker["code"], "printed": False}
    )
    return {"status": "added"}


@router.post("/api/print_qr/confirm")
async def confirm_printed(code: str):
    await db.print_queue.update_one(
        {"code": code, "printed": False}, {"$set": {"printed": True}}
    )
    return {"status": "ok"}
