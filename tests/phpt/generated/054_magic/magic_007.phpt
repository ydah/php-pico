--TEST--
magic get set and string conversion vector 007
--FILE--
<?php
class GeneratedMagic007 {
    private $value = 79;
    public function __get($name) { return $name . ':' . $this->value; }
    public function __set($name, $value) { $this->value = $value; }
    public function __toString() { return 'box=' . $this->value; }
}
$box = new GeneratedMagic007(); echo $box->missing, ':';
$box->dynamic = 82; echo $box;
--EXPECT--
missing:79:box=82
