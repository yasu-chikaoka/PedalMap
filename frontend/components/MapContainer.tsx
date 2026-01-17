'use client';

import { Map, Marker, InfoWindow } from '@vis.gl/react-google-maps';
import { Plus, MapPin } from 'lucide-react';
import { RoutePolyline } from '@/components/RoutePolyline';
import { UI_TEXT, APP_CONFIG } from '@/config/constants';
import type { RouteResponse, Stop, Location } from '@/types';

interface MapContainerProps {
  hasApiKey: boolean;
  mapCenter: Location;
  setMapCenter: (loc: Location) => void;
  routeData: RouteResponse | null;
  selectedSpot: Stop | null;
  setSelectedSpot: (stop: Stop | null) => void;
  onSpotClick: (stop: Stop) => void;
  onAddWaypoint: (stop: Stop) => void;
}

export const MapContainer = ({
  hasApiKey,
  mapCenter,
  setMapCenter,
  routeData,
  selectedSpot,
  setSelectedSpot,
  onSpotClick,
  onAddWaypoint,
}: MapContainerProps) => {
  if (!hasApiKey) {
    return (
      <div className="w-full h-full bg-gray-100 flex items-center justify-center">
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
      </div>
    );
  }

  return (
    <div className="w-full h-full bg-gray-100 relative">
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
            onClick={() => onSpotClick(stop)}
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
                onClick={() => onAddWaypoint(selectedSpot)}
                className="w-full bg-blue-600 text-white text-xs px-2 py-1.5 rounded hover:bg-blue-700 flex items-center justify-center gap-1 transition-colors"
              >
                <Plus size={12} />
                {UI_TEXT.BUTTONS.ADD_WAYPOINT}
              </button>
            </div>
          </InfoWindow>
        )}
      </Map>
    </div>
  );
};
