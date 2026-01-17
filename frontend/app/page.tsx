'use client';

import { useState, useCallback, useEffect } from 'react';
import { APIProvider } from '@vis.gl/react-google-maps';
import { Toaster, toast } from 'react-hot-toast';
import { ControlPanel } from '@/components/ControlPanel';
import { MapContainer } from '@/components/MapContainer';
import { APP_CONFIG, UI_TEXT } from '@/config/constants';
import { useRoute } from '@/hooks/useRoute';
import type { Waypoint, Stop, Location } from '@/types';

export default function Home() {
  const [startPoint, setStartPoint] = useState<Location>(
    APP_CONFIG.DEFAULT_LOCATIONS.START,
  );
  const [endPoint, setEndPoint] = useState<Location>(
    APP_CONFIG.DEFAULT_LOCATIONS.END,
  );
  const [waypoints, setWaypoints] = useState<Waypoint[]>([]);
  const [startPlaceName, setStartPlaceName] = useState(
    APP_CONFIG.DEFAULT_LOCATIONS.START.name || '',
  );
  const [endPlaceName, setEndPlaceName] = useState(
    APP_CONFIG.DEFAULT_LOCATIONS.END.name || '',
  );
  const [targetDistance, setTargetDistance] = useState<number>(
    APP_CONFIG.ROUTES.DEFAULT_DISTANCE_KM,
  );
  const [targetElevation, setTargetElevation] = useState<number>(
    APP_CONFIG.ROUTES.DEFAULT_ELEVATION_M,
  );
  const [selectedSpot, setSelectedSpot] = useState<Stop | null>(null);
  const [mapCenter, setMapCenter] = useState(APP_CONFIG.DEFAULT_MAP_CENTER);

  // Custom Hook
  const { routeData, loading, error, generateRoute } = useRoute({
    startPoint,
    endPoint,
    waypoints,
    targetDistance,
    targetElevation,
  });

  // Update map center when start point changes
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

      const newWaypoints = [...waypoints, newWaypoint];
      setWaypoints(newWaypoints);
      setSelectedSpot(null);
      toast.success('経由地を追加しました');

      // Auto regenerate route
      generateRoute(newWaypoints);
    },
    [waypoints, generateRoute],
  );

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
      },
    );
  }, []);

  const hasApiKey =
    APP_CONFIG.GOOGLE_MAPS.API_KEY &&
    APP_CONFIG.GOOGLE_MAPS.API_KEY !== 'YOUR_API_KEY_HERE';

  const content = (
    <>
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
        onGenerate={() => generateRoute()}
        onGetCurrentLocation={getCurrentLocation}
        onSpotClick={handleSpotClick}
      />

      <div className="w-full md:w-2/3 h-full">
        <MapContainer
          hasApiKey={!!hasApiKey}
          mapCenter={mapCenter}
          setMapCenter={setMapCenter}
          routeData={routeData}
          selectedSpot={selectedSpot}
          setSelectedSpot={setSelectedSpot}
          onSpotClick={handleSpotClick}
          onAddWaypoint={handleAddWaypoint}
        />
      </div>
    </>
  );

  return (
    <div className="flex h-screen w-full flex-col md:flex-row">
      <Toaster position="top-right" />

      {hasApiKey ? (
        <APIProvider
          apiKey={APP_CONFIG.GOOGLE_MAPS.API_KEY}
          libraries={APP_CONFIG.GOOGLE_MAPS.LIBRARIES}
        >
          {content}
        </APIProvider>
      ) : (
        content
      )}

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
