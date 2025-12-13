import { useState } from 'react';
import {
  DndContext,
  closestCenter,
  KeyboardSensor,
  PointerSensor,
  useSensor,
  useSensors,
  DragEndEvent,
} from '@dnd-kit/core';
import {
  arrayMove,
  SortableContext,
  sortableKeyboardCoordinates,
  verticalListSortingStrategy,
  useSortable,
} from '@dnd-kit/sortable';
import { CSS } from '@dnd-kit/utilities';
import { GripVertical, Trash2, Plus } from 'lucide-react';
import { PlaceAutocomplete } from './PlaceAutocomplete';

interface Waypoint {
  id: string;
  name: string;
  location: { lat: number; lng: number };
}

import { Dispatch, SetStateAction } from 'react';

interface WaypointsListProps {
  waypoints: Waypoint[];
  setWaypoints: Dispatch<SetStateAction<Waypoint[]>>;
  apiKey: string;
}

interface SortableItemProps {
  id: string;
  waypoint: Waypoint;
  onRemove: (id: string) => void;
  onPlaceSelect: (id: string, place: google.maps.places.PlaceResult) => void;
  apiKey: string;
}

const SortableItem = ({
  id,
  waypoint,
  onRemove,
  onPlaceSelect,
  apiKey,
}: SortableItemProps) => {
  const { attributes, listeners, setNodeRef, transform, transition } =
    useSortable({ id });

  const style = {
    transform: CSS.Transform.toString(transform),
    transition,
  };

  return (
    <div
      ref={setNodeRef}
      style={style}
      className="flex items-center gap-2 mb-2 bg-white p-2 rounded border border-gray-200"
    >
      <div
        {...attributes}
        {...listeners}
        className="cursor-grab text-gray-400 hover:text-gray-600"
      >
        <GripVertical size={20} />
      </div>
      <div className="flex-1">
        {apiKey ? (
          <PlaceAutocomplete
            onPlaceSelect={(place) => onPlaceSelect(id, place)}
            placeholder="経由地を入力"
            value={waypoint.name}
            onChange={(val) => {
              /* テキスト変更だけなら親の状態は更新しない（Autocomplete選択で更新） */
            }}
          />
        ) : (
          <input
            type="text"
            className="w-full p-1 border rounded text-sm text-gray-900"
            value={`Lat: ${waypoint.location.lat.toFixed(4)}, Lng: ${waypoint.location.lng.toFixed(4)}`}
            readOnly
          />
        )}
      </div>
      <button
        onClick={() => onRemove(id)}
        className="text-red-400 hover:text-red-600 p-1"
      >
        <Trash2 size={18} />
      </button>
    </div>
  );
};

export const WaypointsList = ({
  waypoints,
  setWaypoints,
  apiKey,
}: WaypointsListProps) => {
  const sensors = useSensors(
    useSensor(PointerSensor),
    useSensor(KeyboardSensor, {
      coordinateGetter: sortableKeyboardCoordinates,
    }),
  );

  const handleDragEnd = (event: DragEndEvent) => {
    const { active, over } = event;

    if (over && active.id !== over.id) {
      setWaypoints((items: Waypoint[]) => {
        const oldIndex = items.findIndex((item) => item.id === active.id);
        const newIndex = items.findIndex((item) => item.id === over.id);
        return arrayMove(items, oldIndex, newIndex);
      });
    }
  };

  const addWaypoint = () => {
    const newWaypoint: Waypoint = {
      id: crypto.randomUUID(),
      name: '',
      location: { lat: 0, lng: 0 }, // 初期値
    };
    setWaypoints([...waypoints, newWaypoint]);
  };

  const removeWaypoint = (id: string) => {
    setWaypoints(waypoints.filter((wp) => wp.id !== id));
  };

  const updateWaypoint = (
    id: string,
    place: google.maps.places.PlaceResult,
  ) => {
    if (!place.geometry?.location) return;

    setWaypoints(
      waypoints.map((wp) => {
        if (wp.id === id) {
          return {
            ...wp,
            name: place.name || '',
            location: {
              lat: place.geometry!.location!.lat(),
              lng: place.geometry!.location!.lng(),
            },
          };
        }
        return wp;
      }),
    );
  };

  return (
    <div className="space-y-2">
      <div className="flex justify-between items-center mb-2">
        <label className="text-sm text-gray-600">経由地</label>
        <button
          onClick={addWaypoint}
          className="text-blue-600 text-xs flex items-center gap-1 hover:underline"
        >
          <Plus size={14} /> 追加
        </button>
      </div>

      <DndContext
        sensors={sensors}
        collisionDetection={closestCenter}
        onDragEnd={handleDragEnd}
      >
        <SortableContext
          items={waypoints.map((wp) => wp.id)}
          strategy={verticalListSortingStrategy}
        >
          {waypoints.map((wp) => (
            <SortableItem
              key={wp.id}
              id={wp.id}
              waypoint={wp}
              onRemove={removeWaypoint}
              onPlaceSelect={updateWaypoint}
              apiKey={apiKey}
            />
          ))}
        </SortableContext>
      </DndContext>

      {waypoints.length === 0 && (
        <div className="text-center text-xs text-gray-400 py-2 border border-dashed rounded bg-gray-50">
          経由地がありません
        </div>
      )}
    </div>
  );
};
