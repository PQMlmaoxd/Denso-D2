from denso_d2.shared import Action, FactoryConfig

def test_contracts_construct():
    config = FactoryConfig(name="test")
    action = Action("a1", "TEST", "target", 1)
    assert config.name == "test"
    assert action.action_id == "a1"
