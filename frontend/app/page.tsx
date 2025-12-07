'use client';

import { useState, useCallback } from 'react';
import { MapPin, Navigation, Clock, Activity } from 'lucide-react';
import { APIProvider, Map } from '@vis.gl/react-google-maps';
import { RoutePolyline } from '@/components/RoutePolyline';
import { PlaceAutocomplete } from '@/components/PlaceAutocomplete';

// APIキー設定
// ここにAPIキーを設定するか、環境変数 NEXT_PUBLIC_GOOGLE_MAPS_API_KEY を使用します
const GOOGLE_MAPS_API_KEY = process.env.NEXT_PUBLIC_GOOGLE_MAPS_API_KEY || '';

// APIの型定義
interface RouteSummary {
  total_distance_m: number;
  estimated_moving_time_s: number;
}

interface Stop {
  name: string;
  type: string;
  rating: number;
  location: { lat: number; lon: number };
}

interface RouteResponse {
  summary: RouteSummary;
  geometry: string; // Encoded Polyline
  stops?: Stop[];
}

export default function Home() {
  const [startPoint, setStartPoint] = useState({ lat: 35.681236, lng: 139.767125 }); // 東京駅
  const [endPoint, setEndPoint] = useState({ lat: 35.685175, lng: 139.7528 });   // 皇居
  const [startPlaceName, setStartPlaceName] = useState('東京駅');
  const [endPlaceName, setEndPlaceName] = useState('皇居');
  const [routeData, setRouteData] = useState<RouteResponse | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const handleSearch = async () => {
    setLoading(true);
    setError(null);
    setRouteData(null);

    try {
      const response = await fetch('http://localhost:8080/api/v1/route/generate', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          start_point: { lat: startPoint.lat, lon: startPoint.lng },
          end_point: { lat: endPoint.lat, lon: endPoint.lng },
        }),
      });

      if (!response.ok) {
        throw new Error('ルート検索に失敗しました');
      }

      const data = await response.json();
      setRouteData(data);
    } catch (err) {
      setError(err instanceof Error ? err.message : '予期せぬエラーが発生しました');
    } finally {
      setLoading(false);
    }
  };

  const handleStartPlaceSelect = useCallback((place: google.maps.places.PlaceResult) => {
    if (place.geometry?.location) {
      setStartPoint({
        lat: place.geometry.location.lat(),
        lng: place.geometry.location.lng(),
      });
      if (place.name) {
        setStartPlaceName(place.name);
      }
    }
  }, []);

  const handleEndPlaceSelect = useCallback((place: google.maps.places.PlaceResult) => {
    if (place.geometry?.location) {
      setEndPoint({
        lat: place.geometry.location.lat(),
        lng: place.geometry.location.lng(),
      });
      if (place.name) {
        setEndPlaceName(place.name);
      }
    }
  }, []);

  const hasApiKey = GOOGLE_MAPS_API_KEY && GOOGLE_MAPS_API_KEY !== 'YOUR_API_KEY_HERE';

  // コンテンツ部分 (APIProviderでラップするかどうかで分岐するためコンポーネント化推奨だが、ここでは変数として扱う)
  const ControlPanel = () => (
    <div className="w-full md:w-1/3 p-6 bg-white shadow-lg z-10 overflow-y-auto h-full">
      <h1 className="text-2xl font-bold mb-6 text-gray-800 flex items-center gap-2">
        <Activity className="text-blue-600" />
        Cycling Route Generator
      </h1>

      <div className="space-y-6">
        <div className="space-y-4">
          <div className="p-4 bg-gray-50 rounded-lg border border-gray-200">
            <h2 className="font-semibold text-gray-700 mb-3 flex items-center gap-2">
              <MapPin size={18} /> 出発・目的地
            </h2>
            <div className="space-y-3">
              <div>
                <label className="block text-sm text-gray-600 mb-1">Start</label>
                {hasApiKey ? (
                  <PlaceAutocomplete
                    onPlaceSelect={handleStartPlaceSelect}
                    placeholder="出発地を検索 (例: 東京駅)"
                    value={startPlaceName}
                    onChange={setStartPlaceName}
                  />
                ) : (
                  <div className="flex gap-2">
                    <input
                      type="number"
                      value={startPoint.lat}
                      onChange={(e) => setStartPoint({ ...startPoint, lat: parseFloat(e.target.value) })}
                      className="w-full p-2 border rounded text-sm"
                      step="0.000001"
                      placeholder="Lat"
                    />
                    <input
                      type="number"
                      value={startPoint.lng}
                      onChange={(e) => setStartPoint({ ...startPoint, lng: parseFloat(e.target.value) })}
                      className="w-full p-2 border rounded text-sm"
                      step="0.000001"
                      placeholder="Lng"
                    />
                  </div>
                )}
                {hasApiKey && (
                  <div className="text-xs text-gray-400 mt-1">
                    Lat: {startPoint.lat.toFixed(6)}, Lng: {startPoint.lng.toFixed(6)}
                  </div>
                )}
              </div>
              <div>
                <label className="block text-sm text-gray-600 mb-1">End</label>
                {hasApiKey ? (
                  <PlaceAutocomplete
                    onPlaceSelect={handleEndPlaceSelect}
                    placeholder="目的地を検索 (例: 皇居)"
                    value={endPlaceName}
                    onChange={setEndPlaceName}
                  />
                ) : (
                  <div className="flex gap-2">
                    <input
                      type="number"
                      value={endPoint.lat}
                      onChange={(e) => setEndPoint({ ...endPoint, lat: parseFloat(e.target.value) })}
                      className="w-full p-2 border rounded text-sm"
                      step="0.000001"
                      placeholder="Lat"
                    />
                    <input
                      type="number"
                      value={endPoint.lng}
                      onChange={(e) => setEndPoint({ ...endPoint, lng: parseFloat(e.target.value) })}
                      className="w-full p-2 border rounded text-sm"
                      step="0.000001"
                      placeholder="Lng"
                    />
                  </div>
                )}
                {hasApiKey && (
                  <div className="text-xs text-gray-400 mt-1">
                    Lat: {endPoint.lat.toFixed(6)}, Lng: {endPoint.lng.toFixed(6)}
                  </div>
                )}
              </div>
            </div>
          </div>

          <button
            onClick={handleSearch}
            disabled={loading}
            className="w-full py-3 bg-blue-600 hover:bg-blue-700 text-white font-bold rounded-lg transition-colors flex items-center justify-center gap-2 disabled:opacity-50"
          >
            {loading ? (
              'Searching...'
            ) : (
              <>
                <Navigation size={20} />
                ルートを生成
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
          <div className="p-4 bg-green-50 border border-green-200 rounded-lg space-y-3">
            <h3 className="font-bold text-green-800 border-b border-green-200 pb-2">ルート生成結果</h3>
            <div className="grid grid-cols-2 gap-4">
              <div>
                <p className="text-xs text-gray-500 uppercase">Total Distance</p>
                <p className="text-xl font-mono font-bold text-gray-800">
                  {(routeData.summary.total_distance_m / 1000).toFixed(2)} <span className="text-sm font-normal">km</span>
                </p>
              </div>
              <div>
                <p className="text-xs text-gray-500 uppercase">Est. Time</p>
                <p className="text-xl font-mono font-bold text-gray-800 flex items-center gap-1">
                  <Clock size={16} className="text-gray-400" />
                  {Math.floor(routeData.summary.estimated_moving_time_s / 60)} <span className="text-sm font-normal">min</span>
                </p>
              </div>
            </div>
            
            {routeData.stops && routeData.stops.length > 0 && (
              <div className="mt-4 border-t border-green-200 pt-3">
                <h4 className="font-semibold text-green-800 mb-2 text-sm flex items-center gap-1">
                  <MapPin size={14} /> おすすめ経由スポット
                </h4>
                <ul className="space-y-2">
                  {routeData.stops.map((stop, i) => (
                    <li key={i} className="bg-white p-2 rounded border border-gray-100 text-sm shadow-sm">
                      <div className="font-bold text-gray-700">{stop.name}</div>
                      <div className="flex justify-between text-xs text-gray-500 mt-1">
                        <span className="bg-blue-100 text-blue-800 px-1.5 py-0.5 rounded capitalize">{stop.type}</span>
                        <span className="flex items-center gap-0.5 text-orange-500 font-bold">★ {stop.rating}</span>
                      </div>
                    </li>
                  ))}
                </ul>
              </div>
            )}
          </div>
        )}
      </div>
    </div>
  );

  return (
    <div className="flex h-screen w-full flex-col md:flex-row">
      {hasApiKey ? (
        <APIProvider apiKey={GOOGLE_MAPS_API_KEY} libraries={['places', 'geometry']}>
          <ControlPanel />
          <div className="w-full md:w-2/3 bg-gray-100 flex items-center justify-center relative">
            <Map
              className="w-full h-full"
              defaultCenter={startPoint}
              defaultZoom={13}
              gestureHandling={'greedy'}
              disableDefaultUI={true}
            >
              {routeData?.geometry && <RoutePolyline encodedGeometry={routeData.geometry} />}
            </Map>
          </div>
        </APIProvider>
      ) : (
        <>
          <ControlPanel />
          <div className="w-full md:w-2/3 bg-gray-100 flex items-center justify-center relative">
            <div className="text-center p-8">
              <MapPin size={48} className="mx-auto text-gray-400 mb-4" />
              <h2 className="text-xl font-semibold text-gray-600">Google Maps Area</h2>
              <p className="text-gray-500 mt-2">APIキーが設定されていません。</p>
              <p className="text-sm text-gray-400 mt-4">
                <code>.env.local</code> に <code>NEXT_PUBLIC_GOOGLE_MAPS_API_KEY</code> を設定すると地図と地名検索が有効になります。
              </p>
            </div>
          </div>
        </>
      )}
      
      {/* Footer / Attribution */}
      <div className="absolute bottom-0 right-0 bg-white/80 px-2 py-1 text-[10px] text-gray-500 z-0 pointer-events-none">
        Map data © Google, Route data © <a href="https://www.openstreetmap.org/copyright" target="_blank" rel="noreferrer" className="pointer-events-auto hover:underline">OpenStreetMap</a> contributors
      </div>
    </div>
  );
}
