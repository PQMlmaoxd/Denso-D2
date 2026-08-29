from denso_d2.integration import run_vertical_slice

def test_vertical_slice_runs():
    result = run_vertical_slice()
    assert result["factory"] == "toy-factory"
    assert "simulation" in result
    assert "recommendations" in result
