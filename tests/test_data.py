from denso_d2.data import make_synthetic_config


def test_make_synthetic_config_defaults():
    config = make_synthetic_config()
    assert config.name == "toy-factory"
    assert config.parameters["transporter_count"] == 1
