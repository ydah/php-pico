--TEST--
private and protected member access vector 002
--FILE--
<?php
class GeneratedVisibility002 {
    private $left = 18; protected $right = 16;
    private function left() { return $this->left; }
    protected function right() { return $this->right; }
    public function total() { return $this->left() + $this->right(); }
}
echo (new GeneratedVisibility002())->total();
--EXPECT--
34
