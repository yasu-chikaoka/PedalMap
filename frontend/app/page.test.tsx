import { render, screen, fireEvent, waitFor, act } from '@testing-library/react';
import Home from './page';

// Mock child components
jest.mock('@vis.gl/react-google-maps', () => ({
  APIProvider: ({ children }: { children: React.ReactNode }) => (
    <div>{children}</div>
  ),
  Map: ({ children }: { children: React.ReactNode }) => (
    <div>Mock Map {children}</div>
  ),
  useMapsLibrary: () => null,
  useMap: () => null,
}));
jest.mock('@/components/PlaceAutocomplete', () => ({
  PlaceAutocomplete: () => <input data-testid="mock-autocomplete" />,
}));
jest.mock('@/components/RoutePolyline', () => ({
  RoutePolyline: () => <div data-testid="mock-polyline" />,
}));
jest.mock('@/components/WaypointsList', () => ({
  WaypointsList: () => <div data-testid="mock-waypoints" />,
}));

describe('Home Component', () => {
  const originalEnv = process.env;

  beforeEach(() => {
    jest.resetModules();
    process.env = {
      ...originalEnv,
      NEXT_PUBLIC_API_ENDPOINT: 'http://api.example.com/api/v1',
      NEXT_PUBLIC_GOOGLE_MAPS_API_KEY: 'test-api-key',
    };
    global.fetch = jest.fn();
  });

  afterEach(() => {
    process.env = originalEnv;
    jest.clearAllMocks();
  });

  it('renders the main heading', () => {
    render(<Home />);
    const heading = screen.getByRole('heading', { level: 1 });
    expect(heading).toBeInTheDocument();
    expect(heading).toHaveTextContent('Cycling Route Generator');
  });

  it('renders all essential UI elements', () => {
    render(<Home />);
    expect(screen.getAllByTestId('mock-autocomplete')).toHaveLength(2);
    expect(screen.getByTestId('mock-waypoints')).toBeInTheDocument();
    expect(screen.getByRole('button', { name: /ルートを生成/i })).toBeInTheDocument();
  });

  it('successfully fetches and displays route data', async () => {
    const mockApiResponse = {
      summary: {
        total_distance_m: 25000,
        total_elevation_gain_m: 150,
        estimated_moving_time_s: 3600,
      },
      geometry: 'test_polyline',
      stops: [{ lat: 35.6, lon: 139.7, name: 'Test Stop', type: 'cafe', rating: 4 }],
    };

    (global.fetch as jest.Mock).mockResolvedValueOnce({
      ok: true,
      json: async () => mockApiResponse,
    });

    render(<Home />);
    fireEvent.click(screen.getByRole('button', { name: /ルートを生成/i }));

    await waitFor(() => {
      expect(global.fetch).toHaveBeenCalledWith(
        'http://api.example.com/api/v1/route/generate',
        expect.anything()
      );
    });
    
    // Check if summary is displayed
    const distanceElement = await screen.findByText(/Total Distance/i);
    expect(distanceElement.parentElement?.textContent).toContain('25.00 km');

    const elevationElement = screen.getByText(/Elevation Gain/i);
    expect(elevationElement.parentElement?.textContent).toContain('150 m');

    const timeElement = screen.getByText(/Est. Time/i);
    expect(timeElement.parentElement?.textContent).toContain('60 min');
  });

  it('displays an error message when the API call fails', async () => {
    (global.fetch as jest.Mock).mockResolvedValueOnce({
      ok: false,
      status: 500,
      statusText: 'Internal Server Error',
    });

    render(<Home />);
    fireEvent.click(screen.getByRole('button', { name: /ルートを生成/i }));

    expect(await screen.findByText(/ルート検索に失敗しました/)).toBeInTheDocument();
  });

  it('displays a network error message when fetch rejects', async () => {
    (global.fetch as jest.Mock).mockRejectedValueOnce(new TypeError('Network failed'));

    render(<Home />);
    fireEvent.click(screen.getByRole('button', { name: /ルートを生成/i }));

    expect(await screen.findByText(/Network failed/)).toBeInTheDocument();
  });
  
  it('constructs the correct API request payload', async () => {
    (global.fetch as jest.Mock).mockResolvedValue({
      ok: true,
      json: async () => ({ summary: {}, geometry: '', stops: [] }),
    });
    
    render(<Home />);

    // Simulate user input for preferences (example)
    // You would need to add state and handlers for these inputs in Home component
    // fireEvent.change(screen.getByLabelText('Target Distance'), { target: { value: '30' } });

    fireEvent.click(screen.getByRole('button', { name: /ルートを生成/i }));

    await waitFor(() => {
      expect(global.fetch).toHaveBeenCalledWith(
        expect.any(String),
        expect.objectContaining({
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: expect.stringContaining('"waypoints":[]'), // Default state check
        })
      );
    });
  });

  it('shows loading state during API call', async () => {
    let resolveFetch: (value: any) => void = () => {};
    (global.fetch as jest.Mock).mockImplementation(() => 
      new Promise(resolve => {
        resolveFetch = resolve;
      })
    );

    render(<Home />);
    fireEvent.click(screen.getByRole('button', { name: /ルートを生成/i }));

    expect(screen.getByRole('button')).toBeDisabled();
    expect(screen.getByRole('button')).toHaveTextContent(/検索中|Generating|Loading/i);

    await act(async () => {
        resolveFetch({
            ok: true,
            json: async () => ({ summary: {}, geometry: '', stops: [] }),
        });
    });

    await waitFor(() => {
        expect(screen.getByRole('button', { name: /ルートを生成/i })).toBeEnabled();
    });
  });

  it('sends correct parameters including distance and elevation', async () => {
    (global.fetch as jest.Mock).mockResolvedValue({
      ok: true,
      json: async () => ({ summary: {}, geometry: '', stops: [] }),
    });

    render(<Home />);

    const distanceInput = screen.getByLabelText(/Target Distance|距離/i);
    fireEvent.change(distanceInput, { target: { value: '50' } });

    const elevationInput = screen.getByLabelText(/Target Elevation|獲得標高/i);
    fireEvent.change(elevationInput, { target: { value: '500' } });

    fireEvent.click(screen.getByRole('button', { name: /ルートを生成/i }));

    await waitFor(() => {
      expect(global.fetch).toHaveBeenCalledWith(
        expect.any(String),
        expect.objectContaining({
          body: expect.stringContaining('"target_distance_km":50'),
        })
      );
      expect(global.fetch).toHaveBeenCalledWith(
        expect.any(String),
        expect.objectContaining({
            body: expect.stringContaining('"target_elevation_gain_m":500'),
        })
      );
    });
  });
});
