// アプリケーション全体で使用する定数定義

export const APP_CONFIG = {
  NAME: 'Cycling Route Generator',
  DEFAULT_MAP_CENTER: {
    lat: 35.170915, // 名古屋駅
    lng: 136.881537,
  },
  DEFAULT_LOCATIONS: {
    START: {
      lat: 35.170915,
      lng: 136.881537,
      name: '名古屋駅',
    },
    END: {
      lat: 35.181446,
      lng: 136.906398,
      name: '名古屋城',
    },
  },
  API: {
    BASE_URL: process.env.NEXT_PUBLIC_API_ENDPOINT || 'http://localhost:8080',
    ENDPOINTS: {
      GENERATE_ROUTE: '/api/v1/route/generate',
    },
    TIMEOUT_MS: 15000,
  },
  GOOGLE_MAPS: {
    API_KEY: process.env.NEXT_PUBLIC_GOOGLE_MAPS_API_KEY || '',
    LIBRARIES: ['places', 'geometry'] as ('places' | 'geometry')[],
    DEFAULT_ZOOM: 13,
  },
  ROUTES: {
    DEFAULT_DISTANCE_KM: 30,
    DEFAULT_ELEVATION_M: 200,
    MIN_DISTANCE_KM: 5,
    MAX_DISTANCE_KM: 200,
    MIN_ELEVATION_M: 0,
    MAX_ELEVATION_M: 3000,
  },
};

export const UI_TEXT = {
  TITLES: {
    APP_NAME: 'Cycling Route Generator',
    START_END: '出発・目的地',
    ROUTE_RESULT: 'ルート生成結果',
    RECOMMENDED_SPOTS: 'おすすめ経由スポット',
    GOOGLE_MAPS_AREA: 'Google Maps Area',
  },
  LABELS: {
    START: 'Start',
    END: 'End',
    CURRENT_LOCATION: '現在地',
    TARGET_DISTANCE: '希望走行距離 (km)',
    TARGET_ELEVATION: '希望獲得標高 (m)',
    TOTAL_DISTANCE: 'Total Distance',
    EST_TIME: 'Est. Time',
    ELEVATION_GAIN: 'Elevation Gain',
  },
  BUTTONS: {
    SEARCHING: 'Searching...',
    GENERATE_ROUTE: 'ルートを生成',
    ADD_WAYPOINT: '経由地に追加',
  },
  PLACEHOLDERS: {
    START_SEARCH: '出発地を検索 (例: 東京駅)',
    END_SEARCH: '目的地を検索 (例: 皇居)',
  },
  MESSAGES: {
    WAYPOINT_EXISTS: 'このスポットは既に経由地に追加されています。',
    GEOLOCATION_NOT_SUPPORTED: 'Geolocation is not supported by your browser',
    GEOLOCATION_ERROR: '現在地の取得に失敗しました。',
    ROUTE_SEARCH_ERROR: 'ルート検索に失敗しました',
    UNEXPECTED_ERROR: '予期せぬエラーが発生しました',
    NO_SPOTS_FOUND: 'ルート周辺のスポットは見つかりませんでした。',
    API_KEY_MISSING: 'APIキーが設定されていません。',
    API_KEY_INSTRUCTION:
      '.env.local に NEXT_PUBLIC_GOOGLE_MAPS_API_KEY を設定すると地図と地名検索が有効になります。',
  },
};
