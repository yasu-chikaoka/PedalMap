'use client';

import { MapPin, Navigation, Activity } from 'lucide-react';
import { PlaceAutocomplete } from '@/components/PlaceAutocomplete';
import { WaypointsList } from '@/components/WaypointsList';
import { RouteStats } from '@/components/RouteStats';
import { APP_CONFIG, UI_TEXT } from '@/config/constants';
import type { Location, Waypoint, RouteResponse, Stop } from '@/types';

interface ControlPanelProps {
  startPoint: Location;
  setStartPoint: React.Dispatch<React.SetStateAction<Location>>;
  endPoint: Location;
  setEndPoint: React.Dispatch<React.SetStateAction<Location>>;
  waypoints: Waypoint[];
  setWaypoints: React.Dispatch<React.SetStateAction<Waypoint[]>>;
  startPlaceName: string;
  setStartPlaceName: (name: string) => void;
  endPlaceName: string;
  setEndPlaceName: (name: string) => void;
  targetDistance: number;
  setTargetDistance: (dist: number) => void;
  targetElevation: number;
  setTargetElevation: (elev: number) => void;
  loading: boolean;
  error: string | null;
  routeData: RouteResponse | null;
  onGenerate: () => void;
  onGetCurrentLocation: () => void;
  onSpotClick: (stop: Stop) => void;
}

export const ControlPanel = ({
  startPoint,
  setStartPoint,
  endPoint,
  setEndPoint,
  waypoints,
  setWaypoints,
  startPlaceName,
  setStartPlaceName,
  endPlaceName,
  setEndPlaceName,
  targetDistance,
  setTargetDistance,
  targetElevation,
  setTargetElevation,
  loading,
  error,
  routeData,
  onGenerate,
  onGetCurrentLocation,
  onSpotClick,
}: ControlPanelProps) => {
  const hasApiKey =
    APP_CONFIG.GOOGLE_MAPS.API_KEY && APP_CONFIG.GOOGLE_MAPS.API_KEY !== 'YOUR_API_KEY_HERE';

  return (
    <div className="w-full md:w-1/3 p-6 bg-white shadow-lg z-10 overflow-y-auto h-full">
      <h1 className="text-2xl font-bold mb-6 text-gray-800 flex items-center gap-2">
        <Activity className="text-blue-600" />
        {UI_TEXT.TITLES.APP_NAME}
      </h1>

      <div className="space-y-6">
        <div className="space-y-4">
          <div className="p-4 bg-gray-50 rounded-lg border border-gray-200">
            <h2 className="font-semibold text-gray-700 mb-3 flex items-center gap-2">
              <MapPin size={18} /> {UI_TEXT.TITLES.START_END}
            </h2>
            <div className="space-y-3">
              <div>
                <div className="flex justify-between items-center mb-1">
                  <label className="block text-sm text-gray-600">
                    {UI_TEXT.LABELS.START}
                  </label>
                  <button
                    onClick={onGetCurrentLocation}
                    className="text-xs text-blue-600 hover:text-blue-800 flex items-center gap-1"
                    title="現在地を取得"
                  >
                    <Navigation size={12} /> {UI_TEXT.LABELS.CURRENT_LOCATION}
                  </button>
                </div>
                {hasApiKey ? (
                  <PlaceAutocomplete
                    onPlaceSelect={(place) => {
                      if (place.geometry?.location) {
                        setStartPoint({
                          lat: place.geometry.location.lat(),
                          lng: place.geometry.location.lng(),
                        });
                        if (place.name) setStartPlaceName(place.name);
                      }
                    }}
                    placeholder={UI_TEXT.PLACEHOLDERS.START_SEARCH}
                    value={startPlaceName}
                    onChange={setStartPlaceName}
                  />
                ) : (
                  <div className="flex gap-2">
                    <input
                      type="number"
                      value={startPoint.lat}
                      onChange={(e) =>
                        setStartPoint({
                          ...startPoint,
                          lat: parseFloat(e.target.value),
                        })
                      }
                      className="w-full p-2 border rounded text-sm"
                      step="0.000001"
                      placeholder="Lat"
                    />
                    <input
                      type="number"
                      value={startPoint.lng}
                      onChange={(e) =>
                        setStartPoint({
                          ...startPoint,
                          lng: parseFloat(e.target.value),
                        })
                      }
                      className="w-full p-2 border rounded text-sm"
                      step="0.000001"
                      placeholder="Lng"
                    />
                  </div>
                )}
              </div>

              <WaypointsList
                waypoints={waypoints}
                setWaypoints={setWaypoints}
                apiKey={APP_CONFIG.GOOGLE_MAPS.API_KEY}
              />

              <div>
                <label className="block text-sm text-gray-600 mb-1">{UI_TEXT.LABELS.END}</label>
                {hasApiKey ? (
                  <PlaceAutocomplete
                    onPlaceSelect={(place) => {
                      if (place.geometry?.location) {
                        setEndPoint({
                          lat: place.geometry.location.lat(),
                          lng: place.geometry.location.lng(),
                        });
                        if (place.name) setEndPlaceName(place.name);
                      }
                    }}
                    placeholder={UI_TEXT.PLACEHOLDERS.END_SEARCH}
                    value={endPlaceName}
                    onChange={setEndPlaceName}
                  />
                ) : (
                  <div className="flex gap-2">
                    <input
                      type="number"
                      value={endPoint.lat}
                      onChange={(e) =>
                        setEndPoint({
                          ...endPoint,
                          lat: parseFloat(e.target.value),
                        })
                      }
                      className="w-full p-2 border rounded text-sm"
                      step="0.000001"
                      placeholder="Lat"
                    />
                    <input
                      type="number"
                      value={endPoint.lng}
                      onChange={(e) =>
                        setEndPoint({
                          ...endPoint,
                          lng: parseFloat(e.target.value),
                        })
                      }
                      className="w-full p-2 border rounded text-sm"
                      step="0.000001"
                      placeholder="Lng"
                    />
                  </div>
                )}
              </div>

              <div className="grid grid-cols-2 gap-4 pt-2 border-t border-gray-100">
                <div>
                  <label className="block text-xs text-gray-500 mb-1">
                    {UI_TEXT.LABELS.TARGET_DISTANCE}
                  </label>
                  <input
                    type="number"
                    value={targetDistance}
                    onChange={(e) =>
                      setTargetDistance(parseFloat(e.target.value) || 0)
                    }
                    className="w-full p-2 border rounded text-sm bg-gray-50 focus:bg-white transition-colors"
                    min={APP_CONFIG.ROUTES.MIN_DISTANCE_KM}
                    max={APP_CONFIG.ROUTES.MAX_DISTANCE_KM}
                  />
                </div>
                <div>
                  <label className="block text-xs text-gray-500 mb-1">
                    {UI_TEXT.LABELS.TARGET_ELEVATION}
                  </label>
                  <input
                    type="number"
                    value={targetElevation}
                    onChange={(e) =>
                      setTargetElevation(parseFloat(e.target.value) || 0)
                    }
                    className="w-full p-2 border rounded text-sm bg-gray-50 focus:bg-white transition-colors"
                    min={APP_CONFIG.ROUTES.MIN_ELEVATION_M}
                    max={APP_CONFIG.ROUTES.MAX_ELEVATION_M}
                  />
                </div>
              </div>
            </div>
          </div>

          <button
            onClick={onGenerate}
            disabled={loading}
            className="w-full py-3 bg-blue-600 hover:bg-blue-700 text-white font-bold rounded-lg transition-colors flex items-center justify-center gap-2 disabled:opacity-50"
          >
            {loading ? (
              UI_TEXT.BUTTONS.SEARCHING
            ) : (
              <>
                <Navigation size={20} />
                {UI_TEXT.BUTTONS.GENERATE_ROUTE}
              </>
            )}
          </button>
        </div>

        {error && (
          <div className="p-4 bg-red-50 text-red-700 border border-red-200 rounded-lg">
            {error}
          </div>
        )}

        {routeData && (
          <RouteStats routeData={routeData} onSpotClick={onSpotClick} />
        )}
      </div>
    </div>
  );
};
