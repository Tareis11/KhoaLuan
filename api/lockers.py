from db import db
from fastapi import APIRouter
from fastapi.responses import JSONResponse
from motor.motor_asyncio import AsyncIOMotorClient
import os

router = APIRouter()


@router.get("/api/lockers")
async def get_all_lockers():
    try:
        lockers = await db.locker.find().to_list(length=None)

        for locker in lockers:
            locker["_id"] = str(locker["_id"])
            if locker.get("number") != 0:
                locker.pop("temperature", None)

        return {"lockers": lockers}

    except Exception as e:
        print("Error fetching lockers:", e)
        return JSONResponse(content={"error": "Internal Server Error"}, status_code=500)
