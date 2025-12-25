'use client';

import { useState, useCallback, useEffect } from 'react';
import { APIProvider, Map, Marker, InfoWindow } from '@vis.gl/react-google-maps';
import { Toaster, toast } from 'react-hot-toast';
import { Plus, MapPin } from 'lucide-react';
import { RoutePolyline } from '@/components/RoutePolyline';
import { ControlPanel } from '@/components/ControlPanel';
import { APP_CONFIG, UI_TEXT } from '@/config/constants';
import { useRoute } from '@/hooks/useRoute';
import type { Waypoint, Stop, Location } from '@/types';

export default function Home() {
  const [startPoint, setStartPoint] = useState<Location>(APP_CONFIG.DEFAULT_LOCATIONS.START);
  const [endPoint, setEndPoint] = useState<Location>(APP_CONFIG.DEFAULT_LOCATIONS.END);
  const [waypoints, setWaypoints] = useState<Waypoint[]>([]);
  const [startPlaceName, setStartPlaceName] = useState(APP_CONFIG.DEFAULT_LOCATIONS.START.name);
  const [endPlaceName, setEndPlaceName] = useState(APP_CONFIG.DEFAULT_LOCATIONS.END.name);
  const [targetDistance, setTargetDistance] = useState<number>(APP_CONFIG.ROUTES.DEFAULT_DISTANCE_KM);
  const [targetElevation, setTargetElevation] = useState<number>(APP_CONFIG.ROUTES.DEFAULT_ELEVATION_M);
  const [selectedSpot, setSelectedSpot] = useState<Stop | null>(null);
  const [mapCenter, setMapCenter] = useState(APP_CONFIG.DEFAULT_MAP_CENTER);

  // カスタムフックの使用
  const { routeData, loading, error, generateRoute } = useRoute({
    startPoint,
    endPoint,
    waypoints,
    targetDistance,
    targetElevation,
  });

  // StartPointが変わったら地図の中心も移動
  useEffect(() => {
    setMapCenter(startPoint);
  }, [startPoint]);

  const handleSpotClick = useCallback((stop: Stop) => {
    setSelectedSpot(stop);
    setMapCenter({ lat: stop.location.lat, lng: stop.location.lon });
  }, []);

  const handleAddWaypoint = useCallback(
    (stop: Stop) => {
      const exists = waypoints.some(
        (wp) =>
          wp.location.lat === stop.location.lat &&
          wp.location.lng === stop.location.lon,
      );

      if (exists) {
        toast.error(UI_TEXT.MESSAGES.WAYPOINT_EXISTS);
        return;
      }

      const newWaypoint: Waypoint = {
        id: crypto.randomUUID(),
        name: stop.name,
        location: { lat: stop.location.lat, lng: stop.location.lon },
      };
      setWaypoints((prev) => [...prev, newWaypoint]);
      setSelectedSpot(null);
      toast.success('経由地を追加しました');
    },
    [waypoints],
  );

  // 現在地取得処理
  const getCurrentLocation = useCallback(() => {
    if (!navigator.geolocation) {
      toast.error(UI_TEXT.MESSAGES.GEOLOCATION_NOT_SUPPORTED);
      return;
    }

    const toastId = toast.loading('現在地を取得中...');
    navigator.geolocation.getCurrentPosition(
      (position) => {
        const { latitude, longitude } = position.coords;
        setStartPoint((prev) => ({ ...prev, lat: latitude, lng: longitude }));
        setStartPlaceName(UI_TEXT.LABELS.CURRENT_LOCATION);
        toast.success('現在地を取得しました', { id: toastId });
      },
      (err) => {
        console.error('Error getting location:', err);
        toast.error(UI_TEXT.MESSAGES.GEOLOCATION_ERROR, { id: toastId });
      }
    );
  }, []);

  const hasApiKey =
    APP_CONFIG.GOOGLE_MAPS.API_KEY && APP_CONFIG.GOOGLE_MAPS.API_KEY !== 'YOUR_API_KEY_HERE';

  return (
    <div className="flex h-screen w-full flex-col md:flex-row">
      <Toaster position="top-right" />
      
      <ControlPanel
        startPoint={startPoint}
        setStartPoint={setStartPoint}
        endPoint={endPoint}
        setEndPoint={setEndPoint}
        waypoints={waypoints}
        setWaypoints={setWaypoints}
        startPlaceName={startPlaceName}
        setStartPlaceName={setStartPlaceName}
        endPlaceName={endPlaceName}
        setEndPlaceName={setEndPlaceName}
        targetDistance={targetDistance}
        setTargetDistance={setTargetDistance}
        targetElevation={targetElevation}
        setTargetElevation={setTargetElevation}
        loading={loading}
        error={error}
        routeData={routeData}
        onGenerate={generateRoute}
        onGetCurrentLocation={getCurrentLocation}
        onSpotClick={handleSpotClick}
      />

      <div className="w-full md:w-2/3 bg-gray-100 flex items-center justify-center relative">
        {hasApiKey ? (
          <APIProvider
            apiKey={APP_CONFIG.GOOGLE_MAPS.API_KEY}
            libraries={APP_CONFIG.GOOGLE_MAPS.LIBRARIES}
          >
            <Map
              className="w-full h-full"
              center={mapCenter}
              defaultZoom={APP_CONFIG.GOOGLE_MAPS.DEFAULT_ZOOM}
              onCenterChanged={(ev) => setMapCenter(ev.detail.center)}
              gestureHandling={'greedy'}
              disableDefaultUI={true}
            >
              {routeData?.geometry && (
                <RoutePolyline encodedGeometry={routeData.geometry} />
              )}
              {routeData?.stops?.map((stop, i) => (
                <Marker
                  key={i}
                  position={{ lat: stop.location.lat, lng: stop.location.lon }}
                  onClick={() => handleSpotClick(stop)}
                />
              ))}
              {selectedSpot && (
                <InfoWindow
                  position={{
                    lat: selectedSpot.location.lat,
                    lng: selectedSpot.location.lon,
                  }}
                  onCloseClick={() => setSelectedSpot(null)}
                >
                  <div className="p-2 min-w-[150px]">
                    <h3 className="font-bold text-sm mb-1 text-gray-800">
                      {selectedSpot.name}
                    </h3>
                    <div className="flex items-center gap-2 mb-2">
                      <span className="bg-blue-100 text-blue-800 text-xs px-1.5 py-0.5 rounded capitalize">
                        {selectedSpot.type}
                      </span>
                      <span className="text-orange-500 font-bold text-xs">
                        ★ {selectedSpot.rating}
                      </span>
                    </div>
                    <button
                      onClick={() => handleAddWaypoint(selectedSpot)}
                      className="w-full bg-blue-600 text-white text-xs px-2 py-1.5 rounded hover:bg-blue-700 flex items-center justify-center gap-1 transition-colors"
                    >
                      <Plus size={12} />
                      {UI_TEXT.BUTTONS.ADD_WAYPOINT}
                    </button>
                  </div>
                </InfoWindow>
              )}
            </Map>
          </APIProvider>
        ) : (
          <div className="text-center p-8">
            <MapPin size={48} className="mx-auto text-gray-400 mb-4" />
            <h2 className="text-xl font-semibold text-gray-600">
              {UI_TEXT.TITLES.GOOGLE_MAPS_AREA}
            </h2>
            <p className="text-gray-500 mt-2">
              {UI_TEXT.MESSAGES.API_KEY_MISSING}
            </p>
            <p className="text-sm text-gray-400 mt-4">
              {UI_TEXT.MESSAGES.API_KEY_INSTRUCTION}
            </p>
          </div>
        )}
      </div>

      <div className="absolute bottom-0 right-0 bg-white/80 px-2 py-1 text-[10px] text-gray-500 z-0 pointer-events-none">
        Map data © Google, Route data ©{' '}
        <a
          href="https://www.openstreetmap.org/copyright"
          target="_blank"
          rel="noreferrer"
          className="pointer-events-auto hover:underline"
        >
          OpenStreetMap
        </a>{' '}
        contributors
      </div>
    </div>
  );
}
