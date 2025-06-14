from db import db
from fastapi import APIRouter
from fastapi.responses import JSONResponse
from pymongo.errors import PyMongoError

router = APIRouter()


@router.get("/api/lockers")
async def get_all_lockers():
    try:
        lockers = await db.locker.find().sort("number", 1).to_list(length=None)

        for locker in lockers:
            locker["_id"] = str(locker["_id"])
            if locker.get("number") != 0:
                locker.pop("temperature", None)

        return {"lockers": lockers}

    except PyMongoError as e:
        print("Error fetching lockers:", e)
        return JSONResponse(content={"error": "Internal Server Error"}, status_code=500)
