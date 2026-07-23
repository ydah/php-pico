--TEST--
magic get set and string conversion vector 013
--FILE--
<?php
class GeneratedMagic013 {
    private $value = 145;
    public function __get($name) { return $name . ':' . $this->value; }
    public function __set($name, $value) { $this->value = $value; }
    public function __toString() { return 'box=' . $this->value; }
}
$box = new GeneratedMagic013(); echo $box->missing, ':';
$box->dynamic = 148; echo $box;
--EXPECT--
missing:145:box=148
